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
        Engine(uint32_t maxBlockSize, int argc, char** argv, std::string &&projectdir);
        const project::Project& Project() const { return m_Project; }
        ~Engine();
        void SetProject(project::Project &&project);
        void ProcessMessages();
        void SetJackConnections(project::TJackConnections &&con);
        void ApplyJackConnections(bool forceNow);
        utils::NotifySource& OnProjectChanged() { return m_OnProjectChanged; }
        const std::string& ProjectDir() const { return m_ProjectDir; }
        std::string PresetsDir() const { return m_ProjectDir + "/presets"; }
        std::string ProjectFile() const { return m_ProjectDir + "/project.json"; }
        void SaveCurrentPreset(size_t presetindex, const std::string &name);
        void LoadCurrentPreset();
        std::string SavePresetForInstance(lilvutils::Instance &instance);
        void StoreReverbPreset();
        void ChangeReverbLv2Uri(std::string &&uri);
        void LoadProject();
        void SaveProject();

    private:
        void SyncRtData();
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
        std::unique_ptr<PluginInstance> m_ReverbInstance;
        std::vector<Part> m_Parts;
        std::unique_ptr<OptionalUI> m_Ui;
        realtimethread::Data m_CurrentRtData;
        std::vector<std::unique_ptr<jackutils::Port>> m_AudioOutPorts;
        project::TJackConnections m_JackConnections;
        std::optional<std::chrono::steady_clock::time_point> m_LastApplyJackConnectionTime;
        utils::NotifySource m_OnProjectChanged;
        bool m_SafeToDestroy = false;
        std::string m_ProjectDir;
        std::unique_ptr<utils::NotifySink> m_UiClosedSink;
        bool m_NeedCloseUi = false;
    };
}