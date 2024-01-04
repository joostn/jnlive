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

namespace engine
{
    class PluginInstance
    {
    public:
        PluginInstance(const PluginInstance&) = delete;
        PluginInstance& operator=(const PluginInstance&) = delete;
        PluginInstance(PluginInstance&&) = delete;
        PluginInstance& operator=(PluginInstance&&) = delete;
        PluginInstance(std::string &&uri, uint32_t samplerate, const std::optional<size_t> &owningPart, size_t owningInstrumentIndex) : m_Plugin(std::make_unique<lilvutils::Plugin>(lilvutils::Uri(std::move(uri)))), m_Instance(std::make_unique<lilvutils::Instance>(*m_Plugin, samplerate)), m_OwningPart(owningPart), m_OwningInstrumentIndex(owningInstrumentIndex)
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
        Engine(uint32_t maxBlockSize) : m_JackClient {"JN Live", [this](jack_nframes_t nframes){
            m_RtProcessor.Process(nframes);
        }}, 
         m_AudioOutPorts { jackutils::Port("out_l", jackutils::Port::Kind::Audio, jackutils::Port::Direction::Output), jackutils::Port("out_r", jackutils::Port::Kind::Audio, jackutils::Port::Direction::Output) }, m_LilvWorld(m_JackClient.SampleRate(), maxBlockSize)
        {
            jack_activate(jackutils::Client::Static().get());
        }
        const project::Project& Project() const { return m_Project; }
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
        }
        void ProcessMessages()
        {
            m_RtProcessor.ProcessMessagesInMainThread();
            ApplyJackConnections(false);
        }
        void SetJackConnections(project::TJackConnections &&con)
        {
            m_JackConnections = std::move(con);
            ApplyJackConnections(true);
        }
        void ApplyJackConnections(bool forceNow);

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
        void SyncPlugins(std::vector<std::unique_ptr<jackutils::Port>> &midiInPortsToDiscard, std::vector<std::unique_ptr<PluginInstance>> &pluginsToDiscard)
        {
            std::optional<jack_nframes_t> jacksamplerate;
            std::vector<std::unique_ptr<PluginInstance>> ownedPlugins;
            std::vector<std::vector<size_t>> partindex2instrumentindex2ownedpluginindex(Project().Parts().size());
            for(size_t instrumentindex = 0; instrumentindex < Project().Instruments().size(); ++instrumentindex)
            {
                const auto &instrument = Project().Instruments()[instrumentindex];
                size_t sharedpluginindex = 0;
                for(size_t partindex = 0; partindex < Project().Parts().size(); ++partindex)
                {
                    auto &instrumentindex2ownedpluginindex = partindex2instrumentindex2ownedpluginindex[partindex];
                    std::optional<size_t> owningpart;
                    if(!instrument.Shared())
                    {
                        owningpart = partindex;
                    }
                    if(instrument.Shared() && (partindex > 0))
                    {
                        instrumentindex2ownedpluginindex.push_back(sharedpluginindex);
                    }
                    else
                    {
                        std::unique_ptr<PluginInstance> plugin_uq;
                        auto pluginindex = ownedPlugins.size();
                        sharedpluginindex = pluginindex;
                        if(m_OwnedPlugins.size() > pluginindex)
                        {
                            auto &existingownedplugin = m_OwnedPlugins[pluginindex];
                            if(existingownedplugin && (existingownedplugin->Lv2Uri() == instrument.Lv2Uri()) && (existingownedplugin->OwningPart() == owningpart) && (existingownedplugin->OwningInstrumentIndex() == instrumentindex) )
                            {
                                // re-use:
                                plugin_uq = std::move(existingownedplugin);
                            }
                        }
                        if(!plugin_uq)
                        {
                            if(!jacksamplerate)
                            {
                                jacksamplerate = jack_get_sample_rate(jackutils::Client::Static().get());
                            }
                            auto lv2uri = instrument.Lv2Uri();
                            plugin_uq = std::make_unique<PluginInstance>(std::move(lv2uri), *jacksamplerate, owningpart, instrumentindex);
                        }
                        instrumentindex2ownedpluginindex.push_back(ownedPlugins.size());
                        ownedPlugins.push_back(std::move(plugin_uq));
                    }
                }
            }
            std::vector<Part> newparts;
            for(size_t partindex = 0; partindex < Project().Parts().size(); ++partindex)
            {
                std::unique_ptr<jackutils::Port> midiInPort;
                if(partindex < m_Parts.size())
                {
                    midiInPort = std::move(m_Parts[partindex].MidiInPort());
                }
                else
                {
                    midiInPort = std::make_unique<jackutils::Port>("midi_in_" + std::to_string(partindex), jackutils::Port::Kind::Midi, jackutils::Port::Direction::Input);
                }
                std::vector<size_t> pluginindices;
                for(size_t instrumentindex = 0; instrumentindex < Project().Instruments().size(); ++instrumentindex)
                {
                    pluginindices.push_back(partindex2instrumentindex2ownedpluginindex[partindex][instrumentindex]);
                }
                newparts.push_back(Part(std::move(pluginindices), std::move(midiInPort)));
            }
            for(auto &part: m_Parts)
            {
                if(part.MidiInPort())
                {
                    midiInPortsToDiscard.push_back(std::move(part.MidiInPort()));
                }
            }
            for(auto &plugin: m_OwnedPlugins)
            {
                if(plugin)
                {
                    pluginsToDiscard.push_back(std::move(plugin));
                }
            }
            m_OwnedPlugins = std::move(ownedPlugins);
            m_Parts = std::move(newparts);
        }
        realtimethread::Data CalcRtData() const
        {
            if(m_OwnedPlugins.size() > realtimethread::kMaxNumPlugins)
            {
                throw std::runtime_error("m_OwnedPlugins.size() > realtimethread::kMaxNumPlugins");
            }
            if(m_Parts.size() > realtimethread::kMaxNumMidiPorts)
            {
                throw std::runtime_error("m_Parts.size() > realtimethread::kMaxNumMidiPorts");
            }
            realtimethread::Data result;
            for(const auto &ownedplugin: m_OwnedPlugins)
            {
                float amplitude = 0.0f;
                if(ownedplugin->OwningPart())
                {
                    amplitude = Project().Parts()[*ownedplugin->OwningPart()].AmplitudeFactor();
                }
                else
                {
                    for(const auto &part: Project().Parts())
                    {
                        if(part.ActiveInstrumentIndex() == ownedplugin->OwningInstrumentIndex())
                        {
                            amplitude = std::max(part.AmplitudeFactor(), amplitude);
                        }
                    }
                }
                realtimethread::Data::Plugin plugin(&ownedplugin->Instance(), amplitude);
                result.Plugins()[result.NumPlugins()] = plugin;
                result.SetNumPlugins(result.NumPlugins() + 1);
            }
            for(size_t partindex = 0; partindex < m_Parts.size(); ++partindex)
            {
                int pluginindex = -1;
                auto instrumentIndexOrNull = Project().Parts()[partindex].ActiveInstrumentIndex();
                if(instrumentIndexOrNull)
                {
                    pluginindex = (int)m_Parts[partindex].PluginIndices()[*instrumentIndexOrNull];
                }
                auto jackport = m_Parts[partindex].MidiInPort().get()->get();
                realtimethread::Data::TMidiPort midiPort(jackport, pluginindex);
                result.MidiPorts()[result.NumMidiPorts()] = midiPort;
                result.SetNumMidiPorts(result.NumMidiPorts() + 1);
            }
            result.OutputAudioPorts()[0] = m_AudioOutPorts[0].get();
            result.OutputAudioPorts()[1] = m_AudioOutPorts[1].get();
            return result;
        }
        const std::vector<std::unique_ptr<PluginInstance>>& OwnedPlugins() const { return m_OwnedPlugins; }
        const std::vector<Part>& Parts() const { return m_Parts; }

    private:
        project::Project m_Project;
        std::vector<std::unique_ptr<PluginInstance>> m_OwnedPlugins;
        std::vector<Part> m_Parts;
        jackutils::Client m_JackClient;
        lilvutils::World m_LilvWorld;
        realtimethread::Processor m_RtProcessor {8192};
        realtimethread::Data m_CurrentRtData;
        std::array<jackutils::Port, 2> m_AudioOutPorts;
        project::TJackConnections m_JackConnections;
        std::optional<std::chrono::steady_clock::time_point> m_LastApplyJackConnectionTime;

    };
}