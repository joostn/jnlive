#pragma once
#include <string>
#include <memory>
#include <vector>
#include "lilvutils.h"
#include "project.h"
#include "jackutils.h"
#include "realtimethread.h"
#include "engine.h"
#include <chrono>
#include <iostream>

namespace engine
{
    class PluginInstance
    {
    public:
        PluginInstance(const PluginInstance&) = delete;
        PluginInstance& operator=(const PluginInstance&) = delete;
        PluginInstance(PluginInstance&&) = delete;
        PluginInstance& operator=(PluginInstance&&) = delete;
        PluginInstance(std::string &&uri, uint32_t samplerate, const std::optional<size_t> &owningPart, size_t owningInstrumentIndex, const realtimethread::Processor &processor) : m_Plugin(std::make_unique<lilvutils::Plugin>(lilvutils::Uri(std::move(uri)))), m_Instance(std::make_unique<lilvutils::Instance>(*m_Plugin, samplerate, processor.realtimeThreadInterface())), m_OwningPart(owningPart), m_OwningInstrumentIndex(owningInstrumentIndex)
        {
        }
        lilvutils::Plugin& Plugin() const { return *m_Plugin; }
        lilvutils::Instance& Instance() const { return *m_Instance; }
        const std::string& Lv2Uri() const { return Plugin().Lv2Uri(); }
        const std::optional<size_t>& OwningPart() const { return m_OwningPart; }
        const size_t& OwningInstrumentIndex() const { return m_OwningInstrumentIndex; }

    private:
        std::unique_ptr<lilvutils::Plugin> m_Plugin;
        std::unique_ptr<lilvutils::Instance> m_Instance;
        std::optional<size_t> m_OwningPart;
        size_t m_OwningInstrumentIndex;
    };
    class Engine
    {
    public:
        class OptionalUI
        {
        public:
            OptionalUI(lilvutils::Instance &instance, std::unique_ptr<lilvutils::UI> &&ui) : m_Instance(instance), m_Ui(std::move(ui))
            {
            }
            lilvutils::Instance &instance() const { return m_Instance; }
            std::unique_ptr<lilvutils::UI>& ui() { return m_Ui; }
        private:
            lilvutils::Instance &m_Instance;
            std::unique_ptr<lilvutils::UI> m_Ui;
        };
        class Part
        {
        public:
            Part(std::vector<size_t> &&pluginIndices, std::unique_ptr<jackutils::Port> &&midiInPort) : m_PluginIndices(std::move(pluginIndices)), m_MidiInPort(std::move(midiInPort))
            {
            }
            const std::vector<size_t>& PluginIndices() const
            {
                return m_PluginIndices;
            }
            const std::unique_ptr<jackutils::Port>& MidiInPort() const { return m_MidiInPort; }
            std::unique_ptr<jackutils::Port>& MidiInPort() { return m_MidiInPort; }
            
        private:
            std::vector<size_t> m_PluginIndices;
            std::unique_ptr<jackutils::Port> m_MidiInPort;
        };
        Engine(uint32_t maxBlockSize, int argc, char** argv) : m_JackClient {"JN Live", [this](jack_nframes_t nframes){
            m_RtProcessor.Process(nframes);
        }}, m_LilvWorld(m_JackClient.SampleRate(), maxBlockSize, argc, argv)
        {
            {
                auto errcode = jack_set_buffer_size(jackutils::Client::Static().get(), 256);
                if(errcode)
                {
                    throw std::runtime_error("jack_set_buffer_size failed");
                }
            }
            {
                auto errcode = jack_activate(jackutils::Client::Static().get());
                if(errcode)
                {
                    throw std::runtime_error("jack_activate failed");
                }
            }
            m_AudioOutPorts.push_back(std::make_unique<jackutils::Port>("out_l", jackutils::Port::Kind::Audio, jackutils::Port::Direction::Output));
            m_AudioOutPorts.push_back(std::make_unique<jackutils::Port>("out_r", jackutils::Port::Kind::Audio, jackutils::Port::Direction::Output));

        }
        const project::Project& Project() const { return m_Project; }
        ~Engine()
        {
            // this will deferredly delete the plugins and midi in ports that are no longer needed:
            SetProject(project::Project());
            for(auto &port: m_AudioOutPorts)
            {
                auto ptr = port.release();
                m_RtProcessor.DeferredExecuteAfterRoundTrip([ptr](){
                    delete ptr;
                });  
            }
            m_AudioOutPorts.clear();
            m_RtProcessor.DeferredExecuteAfterRoundTrip([this](){
                m_SafeToDestroy = true;
            });
            while(!m_SafeToDestroy)
            {
                ProcessMessages();
            }
            m_JackClient.ShutDown();
        }
        void SetProject(project::Project &&project)
        {
            m_Project = std::move(project);
            std::vector<std::unique_ptr<jackutils::Port>> midiInPortsToDiscard;
            std::vector<std::unique_ptr<PluginInstance>> pluginsToDiscard;
            SyncPlugins(midiInPortsToDiscard, pluginsToDiscard);
            SyncRtData();
            /*
            After syncing, we may have plugins and midi in ports that are no longer needed. We cannot delete these right now, because the real-time thread may still be accessing them. Therefore, we post a message to the realtime thread indicating the objects that can be discarded. The realtime thread will simply post the messages back to the main thread. When we receive those messages in ProcessMessages(), we know it's safe to delete the objects.
            */
            for(auto &midiport: midiInPortsToDiscard)
            {
                auto ptr = midiport.release();
                m_RtProcessor.DeferredExecuteAfterRoundTrip([ptr](){
                    delete ptr;
                });  
            }
            for(auto &plugin: pluginsToDiscard)
            {
                auto ptr = plugin.release();
                m_RtProcessor.DeferredExecuteAfterRoundTrip([ptr](){
                    delete ptr;
                });  
            }
            OnProjectChanged().Notify();
        }
        void ProcessMessages()
        {
            m_RtProcessor.ProcessMessagesInMainThread();
            ApplyJackConnections(false);
            if(m_Ui)
            {
                if(m_Ui->ui())
                {
                    m_Ui->ui()->CallIdle();
                }
            }
        }
        void SetJackConnections(project::TJackConnections &&con)
        {
            m_JackConnections = std::move(con);
            ApplyJackConnections(true);
        }
        void ApplyJackConnections(bool forceNow);
        utils::NotifySource& OnProjectChanged() { return m_OnProjectChanged; }

    private:
        void SyncRtData()
        {
            auto rtdata = CalcRtData();
            if(rtdata != m_CurrentRtData)
            {
                m_CurrentRtData = rtdata;
                m_RtProcessor.SetDataFromMainThread(std::move(rtdata));
            }
        }
        void SyncPlugins(std::vector<std::unique_ptr<jackutils::Port>> &midiInPortsToDiscard, std::vector<std::unique_ptr<PluginInstance>> &pluginsToDiscard);
        realtimethread::Data CalcRtData() const;
        const std::vector<std::unique_ptr<PluginInstance>>& OwnedPlugins() const { return m_OwnedPlugins; }
        const std::vector<Part>& Parts() const { return m_Parts; }

    private:
        jackutils::Client m_JackClient;
        lilvutils::World m_LilvWorld;
        realtimethread::Processor m_RtProcessor {8192};
        project::Project m_Project;
        std::vector<std::unique_ptr<PluginInstance>> m_OwnedPlugins;
        std::vector<Part> m_Parts;
        std::unique_ptr<OptionalUI> m_Ui;
        realtimethread::Data m_CurrentRtData;
        std::vector<std::unique_ptr<jackutils::Port>> m_AudioOutPorts;
        project::TJackConnections m_JackConnections;
        std::optional<std::chrono::steady_clock::time_point> m_LastApplyJackConnectionTime;
        utils::NotifySource m_OnProjectChanged;
        bool m_SafeToDestroy = false;
    };
}