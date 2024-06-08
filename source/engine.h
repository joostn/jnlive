#pragma once
#include <string>
#include <memory>
#include <vector>
#include "lilvutils.h"
#include "project.h"
#include "jackutils.h"
#include "realtimethread.h"
#include "engine.h"
#include "midi.h"
#include <chrono>
#include <iostream>

namespace engine
{
    class Engine;
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
    class PluginInstance
    {
    public:
        PluginInstance(const PluginInstance&) = delete;
        PluginInstance& operator=(const PluginInstance&) = delete;
        PluginInstance(PluginInstance&&) = delete;
        PluginInstance& operator=(PluginInstance&&) = delete;
        PluginInstance(std::string &&uri, uint32_t samplerate, const realtimethread::Processor &processor) : m_Plugin(std::make_unique<lilvutils::Plugin>(lilvutils::Uri(std::move(uri)))), m_Instance(std::make_unique<lilvutils::Instance>(*m_Plugin, samplerate, processor.realtimeThreadInterface()))
        {
        }
        lilvutils::Plugin& Plugin() const { return *m_Plugin; }
        lilvutils::Instance& Instance() const { return *m_Instance; }
        const std::string& Lv2Uri() const { return Plugin().Lv2Uri(); }
        const std::unique_ptr<OptionalUI>& Ui() const { return m_Ui; }
        void SetUi(std::unique_ptr<OptionalUI>&& ui) { m_Ui = std::move(ui); }

    private:
        std::unique_ptr<lilvutils::Plugin> m_Plugin;
        std::unique_ptr<lilvutils::Instance> m_Instance;
        std::unique_ptr<OptionalUI> m_Ui;
    };
    class PluginInstanceForPart
    {
    public:
        PluginInstanceForPart(std::string &&uri, uint32_t samplerate, const std::optional<size_t> &owningPart, size_t owningInstrumentIndex, const realtimethread::Processor &processor) : m_PluginInstance(std::make_unique<PluginInstance>(std::move(uri), samplerate,  processor)), m_OwningInstrumentIndex(owningInstrumentIndex), m_OwningPart(owningPart)
        {
        }
        const std::unique_ptr<PluginInstance>& pluginInstance() const { return m_PluginInstance; }
        std::unique_ptr<PluginInstance>& pluginInstance() { return m_PluginInstance; }
        const size_t& OwningInstrumentIndex() const { return m_OwningInstrumentIndex; }
        const std::optional<size_t>& OwningPart() const { return m_OwningPart; }

    private:
        std::unique_ptr<PluginInstance> m_PluginInstance;
        std::optional<size_t> m_OwningPart;
        size_t m_OwningInstrumentIndex;
    };
    class TAuxInPortBase
    {
        friend class TAuxInPortLink;
    public:
        TAuxInPortBase(TAuxInPortBase&&) = delete;
        TAuxInPortBase& operator=(TAuxInPortBase&&) = delete;
        TAuxInPortBase(const TAuxInPortBase&) = delete;
        TAuxInPortBase& operator=(const TAuxInPortBase&) = delete;
        TAuxInPortBase(Engine& engine, std::string &&name);
        virtual ~TAuxInPortBase();
        const std::string &Name() const { return m_Name; }
        Engine* engine() const { return m_Engine; }
    protected:
        virtual void OnMidi(const midi::TMidiOrSysexEvent &event) = 0;  // called in main thread

    private:
        Engine* m_Engine;
        std::string m_Name;
    };
    class TAuxInPortLink
    {
    public:
        TAuxInPortLink(TAuxInPortLink&&) = delete;
        TAuxInPortLink& operator=(TAuxInPortLink&&) = delete;
        TAuxInPortLink(const TAuxInPortLink&) = delete;
        TAuxInPortLink& operator=(const TAuxInPortLink&) = delete;
        TAuxInPortLink(TAuxInPortBase *inport) : m_AuxInPort(inport), m_Port(std::string(inport->Name()), jackutils::Port::Kind::Midi, jackutils::Port::Direction::Input), m_OnMidiCallback([this](const midi::TMidiOrSysexEvent &event){ this->OnMidi(event); })
        {
        }
        jackutils::Port& Port() { return m_Port; }
        const jackutils::Port& Port() const { return m_Port; }
        void OnMidi(const midi::TMidiOrSysexEvent &event)
        {
            if(m_AuxInPort)
            {
                m_AuxInPort->OnMidi(event);
            }
        }
        void Detach()
        {
            m_AuxInPort = nullptr;
        }
        TAuxInPortBase* AuxInPort() const { return m_AuxInPort; }
        bool IsAttached() const { return m_AuxInPort != nullptr; }
        const std::function<void(const midi::TMidiOrSysexEvent &event)>* OnMidiCallback() const { return &m_OnMidiCallback; }

    private:
        jackutils::Port m_Port;
        TAuxInPortBase *m_AuxInPort;
        std::function<void(const midi::TMidiOrSysexEvent &event)> m_OnMidiCallback;
    };

    class TAuxOutPortBase
    {
        friend class TAuxOutPortLink;
    public:
        TAuxOutPortBase(TAuxOutPortBase&&) = delete;
        TAuxOutPortBase& operator=(TAuxOutPortBase&&) = delete;
        TAuxOutPortBase(const TAuxOutPortBase&) = delete;
        TAuxOutPortBase& operator=(const TAuxOutPortBase&) = delete;
        TAuxOutPortBase(Engine& engine, std::string &&name);
        virtual ~TAuxOutPortBase();
        const std::string &Name() const { return m_Name; }
        void Send(const midi::TMidiOrSysexEvent &event) const;

    private:
        Engine* m_Engine;
        std::string m_Name;
        std::function<void(const midi::TMidiOrSysexEvent &event)> m_MidiSendFunc;
    };
    class TAuxOutPortLink
    {
    public:
        TAuxOutPortLink(TAuxOutPortLink&&) = delete;
        TAuxOutPortLink& operator=(TAuxOutPortLink&&) = delete;
        TAuxOutPortLink(const TAuxOutPortLink&) = delete;
        TAuxOutPortLink& operator=(const TAuxOutPortLink&) = delete;
        TAuxOutPortLink(TAuxOutPortBase *outport, Engine &engine);
        jackutils::Port& Port() { return m_Port; }
        const jackutils::Port& Port() const { return m_Port; }
        void Detach()
        {
            m_AuxOutPort = nullptr;
        }
        TAuxOutPortBase* AuxOutPort() const { return m_AuxOutPort; }
        bool IsAttached() const { return m_AuxOutPort != nullptr; }
\
    private:
        jackutils::Port m_Port;
        TAuxOutPortBase *m_AuxOutPort;
        Engine &m_Engine;
    };

    class Engine
    {
        friend TAuxInPortBase;
        friend TAuxOutPortBase;
        friend TAuxOutPortLink;
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
        Engine(uint32_t maxBlockSize, int argc, char** argv, std::string &&projectdir);
        const project::TProject& Project() const { return m_Project; }
        ~Engine();
        void SetProject(project::TProject &&project);
        void ProcessMessages();
        void SetJackConnections(project::TJackConnections &&con);
        void ApplyJackConnections(bool forceNow);
        utils::NotifySource& OnProjectChanged() { return m_OnProjectChanged; }
        const std::string& ProjectDir() const { return m_ProjectDir; }
        std::string PresetsDir() const { return m_ProjectDir + "/presets"; }
        std::string ProjectFile() const { return m_ProjectDir + "/project.json"; }
        void SaveCurrentPreset(size_t presetindex, const std::string &name);
        std::string SavePresetForInstance(lilvutils::Instance &instance);
        void StoreReverbPreset();
        void ChangeReverbLv2Uri(std::string &&uri);
        void LoadProject();
        void SaveProject();
        std::string ReverbPluginName() const;
        void LoadReverbPreset();
        void DeletePreset(size_t presetindex);
        void AddInstrument(const std::string &uri, bool shared);
        void DeleteInstrument(size_t instrumentindex);
        void SwitchFocusedPartToPreset(size_t presetIndex);

    private:
        void LoadCurrentPreset();
        void SyncRtData();
        void SyncPlugins();
        void SyncPlugins(std::vector<std::unique_ptr<jackutils::Port>> &midiInPortsToDiscard, std::vector<std::unique_ptr<PluginInstance>> &pluginsToDiscard);
        realtimethread::Data CalcRtData() const;
        const std::vector<std::unique_ptr<PluginInstanceForPart>>& OwnedPlugins() const { return m_OwnedPlugins; }
        const std::vector<Part>& Parts() const { return m_Parts; }
        void AuxInPortAdded(TAuxInPortBase *port);
        void AuxInPortRemoved(TAuxInPortBase *port);
        void AuxOutPortAdded(TAuxOutPortBase *port);
        void AuxOutPortRemoved(TAuxOutPortBase *port);
        void SendMidiAsync(const midi::TMidiOrSysexEvent &event, jackutils::Port &port);

    private:
        jackutils::Client m_JackClient;
        lilvutils::World m_LilvWorld;
        realtimethread::Processor m_RtProcessor {8192};
        project::TProject m_Project;
        std::vector<std::unique_ptr<PluginInstanceForPart>> m_OwnedPlugins;
        std::unique_ptr<PluginInstance> m_ReverbInstance;
        std::vector<Part> m_Parts;
//        std::unique_ptr<OptionalUI> m_Ui;
//        std::unique_ptr<OptionalUI> m_UiForReverb;
        realtimethread::Data m_CurrentRtData;
        std::vector<std::unique_ptr<jackutils::Port>> m_AudioOutPorts;
        project::TJackConnections m_JackConnections;
        std::optional<std::chrono::steady_clock::time_point> m_LastApplyJackConnectionTime;
        utils::NotifySource m_OnProjectChanged;
        bool m_SafeToDestroy = false;
        std::string m_ProjectDir;
        std::unique_ptr<utils::NotifySink> m_UiClosedSink;
        std::unique_ptr<utils::NotifySink> m_ReverbUiClosedSink;
        std::vector<std::unique_ptr<TAuxInPortLink>> m_AuxInPorts;
        std::vector<std::unique_ptr<TAuxOutPortLink>> m_AuxOutPorts;
        //bool m_NeedCloseUi = false;
        //bool m_NeedCloseReverbUi = false;
    };

    class TController
    {
    private:
        class TInPort : public engine::TAuxInPortBase
        {
        public:
            TInPort(TController &controller, engine::Engine &m_Engine, std::string &&name) : m_Controller(controller), TAuxInPortBase(m_Engine, std::move(name))
            {
            }
            void OnMidi(const midi::TMidiOrSysexEvent &event) override;

        private:
            TController &m_Controller;
        };
        class TOutPort : public engine::TAuxOutPortBase
        {
        public:
            TOutPort(TController &controller, engine::Engine &m_Engine, std::string &&name) : m_Controller(controller), TAuxOutPortBase(m_Engine, std::move(name))
            {
            }

        private:
            TController &m_Controller;
        };
    public:
        TController(const TController&) = delete;
        TController& operator=(const TController&) = delete;
        TController(TController&&) = delete;
        TController& operator=(TController&&) = delete;
        TController(engine::Engine &engine, std::string &&name) : m_Engine(engine), m_InPort(*this, engine, std::string(name + "_in")), m_OutPort(*this, engine, std::string(name + "_out")), m_OnProjectChanged(m_Engine.OnProjectChanged(), [this](){ this->ProjectChanged(); })
        {
        }
        engine::Engine& Engine() const
        {
            return m_Engine;
        }
        const project::TProject& Project() const
        {
            return Engine().Project();
        }
        void SetProject(project::TProject &&project)
        {
            Engine().SetProject(std::move(project));
        }
        void SendMidi(const midi::TMidiOrSysexEvent &event) const;
    protected:
        virtual void OnMidiIn(const midi::TMidiOrSysexEvent &event) = 0;
        virtual void OnProjectChanged(const project::TProject &prevProject) {}
    
    private:
        void ProjectChanged();

    private:
        engine::Engine &m_Engine;
        TInPort m_InPort;
        TOutPort m_OutPort;
        utils::NotifySink m_OnProjectChanged;
        project::TProject m_LastProject;
    };
}