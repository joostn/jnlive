#include "engine.h"
#include <filesystem>

namespace engine
{
    void Engine::ApplyJackConnections(bool forceNow)
    {
        auto now = std::chrono::steady_clock::now();
        if(m_LastApplyJackConnectionTime && !forceNow)
        {
            if(now - *m_LastApplyJackConnectionTime < std::chrono::milliseconds(1000))
            {
                return;
            }
        }
        m_LastApplyJackConnectionTime = now;
        size_t partindex = 0;
        for(const auto &midiInPortName: m_JackConnections.MidiInputs())
        {
            if(partindex >= m_Parts.size())
            {
                break;
            }
            if(!midiInPortName.empty())
            {
                const auto &part = m_Parts[partindex];
                part.MidiInPort()->LinkToPortByName(midiInPortName);
            }
        }
        for(size_t portindex = 0; portindex < m_AudioOutPorts.size(); ++portindex)
        {
            const auto &audioportname = m_JackConnections.AudioOutputs()[portindex];
            if(!audioportname.empty())
            {
                m_AudioOutPorts[portindex]->LinkToPortByName(audioportname);
            }
        }
    }

    realtimethread::Data Engine::CalcRtData() const
    {
        std::set<int> activePluginIndices;
        for(size_t partindex = 0; partindex < m_Parts.size(); ++partindex)
        {
            auto instrumentIndexOrNull = Project().Parts()[partindex].ActiveInstrumentIndex();
            if(instrumentIndexOrNull)
            {
                auto pluginindex = (int)m_Parts[partindex].PluginIndices()[*instrumentIndexOrNull];
                activePluginIndices.insert(pluginindex);
            }
        }

        std::vector<std::optional<size_t>> ownedPluginIndex2RtPluginIndex;
        std::vector<realtimethread::Data::Plugin> plugins;
        for(size_t ownedPluginIndex = 0; ownedPluginIndex < m_OwnedPlugins.size(); ownedPluginIndex++)
        {
            const auto &ownedplugin = m_OwnedPlugins[ownedPluginIndex];
            bool isActive = activePluginIndices.find(ownedPluginIndex) != activePluginIndices.end();
            if(isActive)
            {
                float amplitude = 0.0f;
                bool doOverridePort;
                if(ownedplugin->OwningPart())
                {
                    amplitude = Project().Parts()[*ownedplugin->OwningPart()].AmplitudeFactor();
                    doOverridePort = false;
                }
                else
                {
                    for(const auto &part: Project().Parts())
                    {
                        if(part.ActiveInstrumentIndex() == ownedplugin->OwningInstrumentIndex())
                        {
                            amplitude = std::max(part.AmplitudeFactor(), amplitude);
                        }
                        doOverridePort = true;
                    }
                }
                LV2_Evbuf_Iterator *midiInBuf = nullptr;
                if(ownedplugin->Plugin().MidiInputIndex())
                {
                    if(auto atomconnection = dynamic_cast<lilvutils::TConnection<lilvutils::TAtomPort>*>(ownedplugin->Instance().Connections().at(*ownedplugin->Plugin().MidiInputIndex()).get()))
                    {
                        midiInBuf = &atomconnection->BufferIterator();
                    }
                }
                ownedPluginIndex2RtPluginIndex.push_back(plugins.size());
                plugins.emplace_back(&ownedplugin->Instance(), amplitude, doOverridePort, midiInBuf);
            }
            else
            {
                ownedPluginIndex2RtPluginIndex.push_back(std::nullopt);
            }
        }
        std::vector<realtimethread::Data::TMidiPort> midiPorts;
        for(size_t partindex = 0; partindex < m_Parts.size(); ++partindex)
        {
            std::optional<int> pluginindex;
            auto instrumentIndexOrNull = Project().Parts()[partindex].ActiveInstrumentIndex();
            if(instrumentIndexOrNull)
            {
                auto ownedpluginindex = pluginindex = m_Parts[partindex].PluginIndices()[*instrumentIndexOrNull];
                if(ownedpluginindex)
                {
                    pluginindex = ownedPluginIndex2RtPluginIndex.at(*ownedpluginindex);
                }
            }
            auto jackport = m_Parts[partindex].MidiInPort().get()->get();
            midiPorts.emplace_back(jackport, pluginindex, Project().Parts()[partindex].MidiChannelForSharedInstruments());
        }
        std::array<jack_port_t*, 2> outputAudioPorts {
            m_AudioOutPorts.size() > 0? m_AudioOutPorts[0]->get() : nullptr,
            m_AudioOutPorts.size() > 1?m_AudioOutPorts[1]->get() : nullptr
        };
        return realtimethread::Data(std::move(plugins), std::move(midiPorts), std::move(outputAudioPorts));
    }

    void Engine::SyncPlugins(std::vector<std::unique_ptr<jackutils::Port>> &midiInPortsToDiscard, std::vector<std::unique_ptr<PluginInstance>> &pluginsToDiscard)
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
                        plugin_uq = std::make_unique<PluginInstance>(std::move(lv2uri), *jacksamplerate, owningpart, instrumentindex, m_RtProcessor);
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
        std::unique_ptr<OptionalUI> optionalui;
        if(Project().ShowUi())
        {

            if(Project().FocusedPart() && (*Project().FocusedPart() < Project().Parts().size()) && Project().Parts()[*Project().FocusedPart()].ActiveInstrumentIndex())
            {
                auto partindex = *Project().FocusedPart();
                const auto &instrumentindex2ownedpluginindex = partindex2instrumentindex2ownedpluginindex[partindex];
                auto instrindex = *Project().Parts()[partindex].ActiveInstrumentIndex();
                if( (instrindex >= 0) && (instrindex < instrumentindex2ownedpluginindex.size()) )
                {
                    auto ownedpluginindex = instrumentindex2ownedpluginindex[instrindex];
                    const auto &ownedplugin = ownedPlugins[ownedpluginindex];
                    if(ownedplugin)
                    {
                        auto &instance = ownedplugin->Instance();
                        if(m_Ui && &m_Ui->instance() == &instance)
                        {
                            optionalui = std::move(m_Ui);
                        }
                        else
                        {
                            std::unique_ptr<lilvutils::UI> ui;
                            try
                            {
                                ui = std::make_unique<lilvutils::UI>(instance);
                                m_UiClosedSink = std::make_unique<utils::NotifySink>(ui->OnClose(), [this](){
                                    m_NeedCloseUi = true;
                                });
                            }
                            catch(std::exception &e)
                            {
                                std::cerr << "Failed to create UI for plugin " << instance.plugin().Lv2Uri() << ": " << e.what() << std::endl;
                            }
                            optionalui = std::make_unique<OptionalUI>(instance, std::move(ui));
                        }
                    }
                }
            }
        }
        m_OwnedPlugins = std::move(ownedPlugins);
        m_Parts = std::move(newparts);
        m_Ui = std::move(optionalui);
    }
    void Engine::LoadCurrentPreset()
    {
        if(Project().FocusedPart())
        {
            auto partindex = *Project().FocusedPart();
            if(partindex <  Project().Parts().size())
            {
                const auto &part = Project().Parts()[partindex];
                if(part.ActivePresetIndex())
                {
                    auto presetindex = *part.ActivePresetIndex();
                    if(presetindex < Project().QuickPresets().size())
                    {
                        const auto &preset = Project().QuickPresets()[presetindex];
                        if(preset)
                        {
                            auto instrumentindex = preset.value().InstrumentIndex();
                            auto pluginindex = m_Parts.at(partindex).PluginIndices().at(instrumentindex);
                            const auto &ownedplugin = m_OwnedPlugins.at(pluginindex);
                            if(ownedplugin)
                            {
                                std::string presetdir = PresetsDir() + "/" + preset->PresetSubDir();
                                auto &instance = ownedplugin->Instance();
                                ownedplugin->Instance().LoadState(presetdir);
                            }
                        }
                    }
                
                }
            }
        }
    }

    void Engine::SaveCurrentPreset(size_t presetindex, const std::string &name)
    {
        if(Project().FocusedPart() && (*Project().FocusedPart() < Project().Parts().size()) && Project().Parts()[*Project().FocusedPart()].ActiveInstrumentIndex())
        {
            auto partindex = *Project().FocusedPart();
            const auto &instrumentindex2ownedpluginindex = m_Parts[partindex].PluginIndices();
            auto instrindex = *Project().Parts()[partindex].ActiveInstrumentIndex();
            if( (instrindex >= 0) && (instrindex < instrumentindex2ownedpluginindex.size()) )
            {
                auto ownedpluginindex = instrumentindex2ownedpluginindex[instrindex];
                const auto &ownedplugin = m_OwnedPlugins[ownedpluginindex];
                if(ownedplugin)
                {
                    std::string dirtemplate = PresetsDir() + "/state_XXXXXX";
                    char* presetDirPtr = mkdtemp(const_cast<char*>(dirtemplate.c_str()));
                    if(!presetDirPtr)
                    {
                        throw std::runtime_error("Failed to create directory for preset");
                    }
                    std::string presetDir = presetDirPtr;
                    // change permissions to 0755:
                    if(chmod(presetDir.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0)
                    {
                        throw std::runtime_error("Failed to change permissions for preset directory");
                    }
                    auto dirToDelete = presetDir;
                    utils::finally fin1([&](){
                        if(!dirToDelete.empty())
                        {
                            std::filesystem::remove_all(dirToDelete);
                        }
                    });
                    auto relativePresetDir = std::filesystem::relative(presetDir, PresetsDir());

                    auto &instance = ownedplugin->Instance();
                    instance.SaveState(presetDir);
                    auto newproject = Project();
                    auto presets = newproject.QuickPresets();
                    if(presetindex >= presets.size())
                    {
                        presets.resize(presetindex + 1);
                    }
                    dirToDelete.clear();
                    if(presets[presetindex])
                    {
                        dirToDelete = PresetsDir() + "/" + presets[presetindex]->PresetSubDir();
                    }
                    presets[presetindex] = project::TQuickPreset(instrindex, std::string(name), std::move(relativePresetDir));
                    newproject.SetPresets(std::move(presets));
                    newproject = newproject.ChangePart(partindex, instrindex, presetindex, newproject.Parts().at(partindex).AmplitudeFactor());
                    SetProject(std::move(newproject));
                    SaveProject();
                }
            }
        }

    }
    Engine::Engine(uint32_t maxBlockSize, int argc, char** argv, std::string &&projectdir) : m_JackClient {"JN Live", [this](jack_nframes_t nframes){
        m_RtProcessor.Process(nframes);
    }}, m_LilvWorld(m_JackClient.SampleRate(), maxBlockSize, argc, argv), m_ProjectDir(std::move(projectdir))
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
        if(!std::filesystem::exists(m_ProjectDir))
        {
            std::filesystem::create_directory(m_ProjectDir);
        }
        LoadProject();
        auto presetsdir = PresetsDir();
        if(!std::filesystem::exists(presetsdir))
        {
            std::filesystem::create_directory(presetsdir);
        }
    }
    void Engine::LoadProject()
    {
        std::string projectfile = ProjectFile();
        if(!std::filesystem::exists(projectfile))
        {
            auto prj = project::Project();
            ProjectToFile(prj, projectfile);
        }
        {
            auto prj = project::ProjectFromFile(projectfile);
            prj = prj.Change(0,false);
            SetProject(std::move(prj));
        }
    }
    void Engine::SaveProject()
    {
        std::string projectfile = ProjectFile();
        ProjectToFile(Project(), projectfile);
    }
    Engine::~Engine()
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
    void Engine::SetProject(project::Project &&project)
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
    void Engine::ProcessMessages()
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
        if(m_NeedCloseUi)
        {
            m_NeedCloseUi = false;
            if(Project().ShowUi())
            {
                SetProject(Project().Change(Project().FocusedPart(), false));
            }
        }
    }
    void Engine::SetJackConnections(project::TJackConnections &&con)
    {
        m_JackConnections = std::move(con);
        ApplyJackConnections(true);
    }
    void Engine::SyncRtData()
    {
        auto rtdata = CalcRtData();
        if(rtdata != m_CurrentRtData)
        {
            m_CurrentRtData = rtdata;
            m_RtProcessor.SetDataFromMainThread(std::move(rtdata));
        }
    }
}