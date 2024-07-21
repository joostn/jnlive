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
#include "utils.h"
#include <chrono>
#include <iostream>

namespace engine
{
    class Engine;
    class PluginInstanceForPart;
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
        PluginInstance(std::string &&uri, uint32_t samplerate, const realtimethread::Processor &processor, lilvutils::Instance::TMidiCallback &&midiCallback) : m_Plugin(std::make_unique<lilvutils::Plugin>(lilvutils::Uri(std::move(uri)))), m_Instance(std::make_unique<lilvutils::Instance>(*m_Plugin, samplerate, processor.realtimeThreadInterface(), std::move(midiCallback)))
        {
        }
        lilvutils::Plugin& Plugin() const { return *m_Plugin; }
        lilvutils::Instance& Instance() const { return *m_Instance; }
        const std::string& Lv2Uri() const { return Plugin().Lv2Uri(); }
        const std::unique_ptr<OptionalUI>& Ui() const { return m_Ui; }
        void SetUi(std::unique_ptr<OptionalUI>&& ui) { m_Ui = std::move(ui); }
        LV2_Evbuf_Iterator* GetMidiInBuf() const;

    private:
        std::unique_ptr<lilvutils::Plugin> m_Plugin;
        std::unique_ptr<lilvutils::Instance> m_Instance;
        std::unique_ptr<OptionalUI> m_Ui;
    };
    class PluginInstanceForPart
    {
    public:
        using TMidiCallback = std::function<void(PluginInstanceForPart *, const midi::TMidiOrSysexEvent &event)>;
        PluginInstanceForPart(std::string &&uri, uint32_t samplerate, const std::optional<size_t> &owningPart, size_t owningInstrumentIndex, const realtimethread::Processor &processor, TMidiCallback &&midiCallback) : m_PluginInstance(std::make_unique<PluginInstance>(std::move(uri), samplerate,  processor, [this, midiCallback = std::move(midiCallback)](const midi::TMidiOrSysexEvent &event) mutable { if(midiCallback) {midiCallback(this, event);}})), m_OwningInstrumentIndex(owningInstrumentIndex), m_OwningPart(owningPart)
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

    private:
        jackutils::Port m_Port;
        TAuxOutPortBase *m_AuxOutPort;
        Engine &m_Engine;
    };

    class TPresetLoader
    {
    public:
        TPresetLoader(TPresetLoader&&) = delete;
        TPresetLoader& operator=(TPresetLoader&&) = delete;
        TPresetLoader(const TPresetLoader&) = delete;
        TPresetLoader& operator=(const TPresetLoader&) = delete;
        TPresetLoader(Engine &engine, PluginInstanceForPart &pluginInstance, const std::string &presetdir);
        bool Finished() const { return m_Finished; }
        PluginInstanceForPart& PluginInstance() { return m_PluginInstance; }
        void Start();

    private:
        void StartLoad();
        void LoadDone();
    
    private:
        Engine &m_Engine;
        utils::TThreadWithEventLoop m_LoaderThread;
        utils::TEventLoopAction m_DidRoundTripAction;
        utils::TEventLoopAction m_LoadDoneAction;
        PluginInstanceForPart &m_PluginInstance;
        std::string m_PresetDir;
        bool m_Finished = false;
    };

    class Engine
    {
        friend TAuxInPortBase;
        friend TAuxOutPortBase;
        friend TAuxOutPortLink;
    public:
        class TData
        {
        public:
            TData() = default;
            const project::TProject& Project() const { return m_Project; }
            const project::THammondData& HammondData() const { return m_HammondData; }
            std::optional<size_t> GuiFocusedPart() const { return m_GuiFocusedPart; }
            bool ShowUi() const { return m_ShowUi; }
            bool ShowReverbUi() const { return m_ShowReverbUi; }
            TData ChangeProject(project::TProject &&project) const
            {
                TData ret = *this;
                ret.m_Project = std::move(project);
                ret.FixFocusedPart();
                return ret;
            }
            TData ChangeHammondData(project::THammondData &&hammonddata) const
            {
                TData ret = *this;
                ret.m_HammondData = std::move(hammonddata);
                return ret;
            }
            TData ChangeGuiFocusedPart(std::optional<size_t> guiFocusedPart) const
            {
                TData ret = *this;
                ret.m_GuiFocusedPart = guiFocusedPart;
                ret.FixFocusedPart();
                return ret;
            }
            TData ChangeShowUi(bool showUi) const
            {
                TData ret = *this;
                ret.m_ShowUi = showUi;
                return ret;
            }
            TData ChangeShowReverbUi(bool showReverbUi) const
            {
                TData ret = *this;
                ret.m_ShowReverbUi = showReverbUi;
                return ret;
            }
            auto Tuple() const
            {
                return std::tie(m_Project, m_HammondData, m_GuiFocusedPart, m_ShowUi, m_ShowReverbUi);
            }
            bool operator==(const TData &other) const
            {
                return Tuple() == other.Tuple();
            }
        private:
            void FixFocusedPart()
            {
                if(m_Project.Parts().empty())
                {
                    m_GuiFocusedPart = std::nullopt;
                }
                else
                {
                    if(m_GuiFocusedPart)
                    {
                        if(*m_GuiFocusedPart >= m_Project.Parts().size())
                        {
                            m_GuiFocusedPart = m_Project.Parts().size()-1;
                        }
                    }
                    else
                    {
                        m_GuiFocusedPart = 0;
                    }
                }
            }
        private:
            project::TProject m_Project;
            project::THammondData m_HammondData;
            std::optional<size_t> m_GuiFocusedPart;
            bool m_ShowUi = false;
            bool m_ShowReverbUi = false;
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
        Engine(uint32_t maxBlockSize, int argc, char** argv, std::string &&projectdir, utils::TEventLoop &eventLoop);
        const project::TProject& Project() const { return Data().Project(); }
        ~Engine();
        void SetProject(project::TProject &&project);
        const TData& Data() const { return m_Data; }
        void SetData(TData &&data);
        void UpdateHammondPlugins(const project::THammondData &olddata, bool forceNow);
        void ProcessMessages();
        void ApplyJackConnections(bool forceNow);
        utils::NotifySource& OnDataChanged() { return m_OnDataChanged; }
        const std::string& ProjectDir() const { return m_ProjectDir; }
        std::string PresetsDir() const { return m_ProjectDir + "/presets"; }
        std::string ProjectFile() const { return m_ProjectDir + "/project.json"; }
        void SaveCurrentPreset(size_t partindex, size_t presetindex, const std::string &name);
        std::string SavePresetForInstance(lilvutils::Instance &instance);
        void StoreReverbPreset();
        void ChangeReverbLv2Uri(std::string &&uri);
        void LoadProject();
        void SaveProject(const project::TProject &project);
        std::string ReverbPluginName() const;
        void LoadReverbPreset();
        void DeletePreset(size_t presetindex);
        void AddInstrument(const std::string &uri, bool shared);
        void DeleteInstrument(size_t instrumentindex);
        void SaveProjectSync();
        void SendMidi(const midi::TMidiOrSysexEvent &event, const PluginInstance &plugininstance);
        void SendMidiToPart(const midi::TMidiOrSysexEvent &event, size_t partindex); // midi channel is ignored!
        void SendMidiToPartInstrument(const midi::TMidiOrSysexEvent &event, size_t partindex, size_t instrumentindex); // midi channel is ignored!
        realtimethread::Processor& RtProcessor() { return m_RtProcessor; }
        utils::TEventLoop &EventLoop() const { return m_EventLoop; }
        void PresetLoaderFinished(TPresetLoader *loader);
        bool IsPluginLoading(PluginInstanceForPart *plugin) const;
        int BufferSize() const { return m_BufferSize; }

    private:
        void LoadPresetForPart(size_t partindex);
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
        bool DoProjectSaveThread();
        void OnMidiFromPlugin(PluginInstanceForPart *sender, const midi::TMidiOrSysexEvent &evt);
        void LoadFirstHammondPreset();
        void CleanupPresetLoaders();
        void StartLoading();
        void LoadJackConnections();

    private:
        jackutils::Client m_JackClient;
        lilvutils::World m_LilvWorld;
        realtimethread::Processor m_RtProcessor {8192};
        std::vector<std::unique_ptr<PluginInstanceForPart>> m_OwnedPlugins;
        std::vector<std::unique_ptr<PluginInstanceForPart>> m_OwnedPluginsToBeDiscardedAfterLoad;
        std::unique_ptr<PluginInstance> m_ReverbInstance;
        std::vector<Part> m_Parts;
        TData m_Data;
//        std::unique_ptr<OptionalUI> m_Ui;
//        std::unique_ptr<OptionalUI> m_UiForReverb;
        realtimethread::Data m_CurrentRtData;
        std::vector<std::unique_ptr<jackutils::Port>> m_AudioOutPorts;
        std::unique_ptr<jackutils::Port> m_VocoderInPort;
        project::TJackConnections m_JackConnections;
        std::optional<std::chrono::steady_clock::time_point> m_LastApplyJackConnectionTime;
        utils::NotifySource m_OnDataChanged;
        bool m_SafeToDestroy = false;
        std::string m_ProjectDir;
        std::unique_ptr<utils::NotifySink> m_UiClosedSink;
        std::unique_ptr<utils::NotifySink> m_ReverbUiClosedSink;
        std::vector<std::unique_ptr<TAuxInPortLink>> m_AuxInPorts;
        std::vector<std::unique_ptr<TAuxOutPortLink>> m_AuxOutPorts;
        //bool m_NeedCloseUi = false;
        //bool m_NeedCloseReverbUi = false;
        std::thread m_ProjectSaveThread;
        std::unique_ptr<project::TProject> m_ProjectToSave;
        std::mutex m_ProjectSaveMutex;
        std::mutex m_SaveProjectNowMutex;
        bool m_Quitting = false;
        std::set<PluginInstance*> m_ProcessingDataFromPlugin;
        utils::TEventLoop &m_EventLoop;
        utils::TEventLoopAction m_CleanupPresetLoadersAction;
        std::vector<std::unique_ptr<TPresetLoader>> m_PresetLoaders;
        std::map<PluginInstanceForPart*, std::string> m_Plugin2LoadQueue;
        int m_BufferSize = 128;
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
        TController(engine::Engine &engine, std::string &&name) : m_Engine(engine), m_InPort(*this, engine, std::string(name + "_in")), m_OutPort(*this, engine, std::string(name + "_out")), m_OnEngineDataChanged(m_Engine.OnDataChanged(), [this](){ this->DataChanged(); })
        {
        }
        engine::Engine& Engine() const
        {
            return m_Engine;
        }
        void SendMidi(const midi::TMidiOrSysexEvent &event) const;
    protected:
        virtual void OnMidiIn(const midi::TMidiOrSysexEvent &event) = 0;
        virtual void OnDataChanged(const engine::Engine::TData &prevData) {}
    
    private:
        void DataChanged();

    private:
        engine::Engine &m_Engine;
        TInPort m_InPort;
        TOutPort m_OutPort;
        utils::NotifySink m_OnEngineDataChanged;
        engine::Engine::TData m_LastData;
    };
}