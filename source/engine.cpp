#include "engine.h"
#include <filesystem>

namespace
{
}

namespace engine
{
    std::string Engine::SavePresetForInstance(lilvutils::Instance &instance)
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
        instance.SaveState(presetDir);
        dirToDelete.clear();
        return presetDir;
    }

    void Engine::SwitchFocusedPartToPreset(size_t presetIndex)
    {
        if(Project().FocusedPart())
        {
            if(Project().Parts().at(*Project().FocusedPart()).ActivePresetIndex() != presetIndex)
            {
                auto newproject = Project().SwitchFocusedPartToPreset(presetIndex);
                SetProject(std::move(newproject));
                OnProjectChanged();
                LoadCurrentPreset();
            }
        }
    }

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
        for(const auto &midiInPortNames: m_JackConnections.MidiInputs())
        {
            if(partindex >= m_Parts.size())
            {
                break;
            }
            const auto &part = m_Parts[partindex];
            part.MidiInPort()->LinkToAnyPortByName(midiInPortNames);
            partindex++;
        }
        for(size_t portindex = 0; portindex < m_AudioOutPorts.size(); ++portindex)
        {
            if(portindex < m_JackConnections.AudioOutputs().size())
            {
                const auto &audioportnames = m_JackConnections.AudioOutputs()[portindex];
                m_AudioOutPorts[portindex]->LinkToAllPortsByName(audioportnames);
            }
        }
        for(const auto &auxinport: m_AuxInPorts)
        {
            if(auxinport && auxinport->AuxInPort())
            {
                const auto &name = auxinport->AuxInPort()->Name();
                for(const auto &portpair: m_JackConnections.ControllerMidiPorts())
                {
                    if(portpair.first == name)
                    {
                        auxinport->Port().LinkToAnyPortByName(portpair.second);
                    }
                }
            }
        }
        for(const auto &auxOutport: m_AuxOutPorts)
        {
            if(auxOutport && auxOutport->AuxOutPort())
            {
                const auto &name = auxOutport->AuxOutPort()->Name();
                for(const auto &portpair: m_JackConnections.ControllerMidiPorts())
                {
                    if(portpair.first == name)
                    {
                        auxOutport->Port().LinkToAllPortsByName(portpair.second);
                    }
                }
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
                if(ownedplugin->pluginInstance()->Plugin().MidiInputIndex())
                {
                    if(auto atomconnection = dynamic_cast<lilvutils::TConnection<lilvutils::TAtomPort>*>(ownedplugin->pluginInstance()->Instance().Connections().at(*ownedplugin->pluginInstance()->Plugin().MidiInputIndex()).get()))
                    {
                        midiInBuf = &atomconnection->BufferIterator();
                    }
                }
                ownedPluginIndex2RtPluginIndex.push_back(plugins.size());
                plugins.emplace_back(&ownedplugin->pluginInstance()->Instance(), amplitude, doOverridePort, midiInBuf);
            }
            else
            {
                ownedPluginIndex2RtPluginIndex.push_back(std::nullopt);
            }
        }
        std::vector<realtimethread::Data::TMidiKeyboardPort> midiPorts;
        for(size_t partindex = 0; partindex < m_Parts.size(); ++partindex)
        {
            std::optional<int> pluginindex;
            auto instrumentIndexOrNull = Project().Parts()[partindex].ActiveInstrumentIndex();
            if(instrumentIndexOrNull && (*instrumentIndexOrNull < m_Parts[partindex].PluginIndices().size()))
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
        std::vector<realtimethread::Data::TMidiAuxInPort> auxInPorts;
        for(const auto &auxport: m_AuxInPorts)
        {
            auxInPorts.emplace_back(auxport->Port().get(), auxport->OnMidiCallback());
        }

        std::vector<realtimethread::Data::TMidiAuxOutPort> auxOutPorts;
        for(const auto &auxport: m_AuxOutPorts)
        {
            auxOutPorts.emplace_back(auxport->Port().get());
        }

        auto reverbinstance = m_ReverbInstance? &m_ReverbInstance->Instance() : nullptr;
        auto reverblevel = Project().Reverb().MixLevel();
        return realtimethread::Data(std::move(plugins), std::move(midiPorts), std::move(auxInPorts), std::move(auxOutPorts), std::move(outputAudioPorts), reverbinstance, reverblevel);
    }

    void Engine::SyncPlugins(std::vector<std::unique_ptr<jackutils::Port>> &midiInPortsToDiscard, std::vector<std::unique_ptr<PluginInstance>> &pluginsToDiscard)
    {
        std::optional<jack_nframes_t> jacksamplerate;
        std::vector<std::unique_ptr<PluginInstanceForPart>> ownedPlugins;
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
                    std::unique_ptr<PluginInstanceForPart> plugin_uq;
                    auto pluginindex = ownedPlugins.size();
                    sharedpluginindex = pluginindex;
                    if(m_OwnedPlugins.size() > pluginindex)
                    {
                        auto &existingownedplugin = m_OwnedPlugins[pluginindex];
                        if(existingownedplugin && (existingownedplugin->pluginInstance()->Lv2Uri() == instrument.Lv2Uri()) && (existingownedplugin->OwningPart() == owningpart) && (existingownedplugin->OwningInstrumentIndex() == instrumentindex) )
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
                        plugin_uq = std::make_unique<PluginInstanceForPart>(std::move(lv2uri), *jacksamplerate, owningpart, instrumentindex, m_RtProcessor);
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
                pluginsToDiscard.push_back(std::move(plugin->pluginInstance()));
                plugin = {};
            }
        }
        for(const auto &ownedplugin: ownedPlugins)
        {
            bool isfocused = false;
            if(Project().FocusedPart())
            {
                if( (ownedplugin->OwningPart() && (*Project().FocusedPart() == *ownedplugin->OwningPart()))
                    || (!ownedplugin->OwningPart()))
                {
                    if(Project().Parts().at(*Project().FocusedPart()).ActiveInstrumentIndex() == ownedplugin->OwningInstrumentIndex())
                    {
                        isfocused = true;
                    }
                }
            }
            bool showgui = isfocused && Project().ShowUi();
            if(showgui)
            {
                if(!ownedplugin->pluginInstance()->Ui())
                {
                    auto &instance = ownedplugin->pluginInstance()->Instance();
                    std::unique_ptr<lilvutils::UI> ui;
                    try
                    {
                        ui = std::make_unique<lilvutils::UI>(instance);
                        m_UiClosedSink = std::make_unique<utils::NotifySink>(ui->OnClose(), [this](){
                            if(Project().ShowUi())
                            {
                                auto newproject = Project().Change(Project().FocusedPart(), false);
                                SetProject(std::move(newproject));
                            }
                        });
                    }
                    catch(std::exception &e)
                    {
                        std::cerr << "Failed to create UI for plugin " << instance.plugin().Lv2Uri() << ": " << e.what() << std::endl;
                    }
                    auto optionalui = std::make_unique<OptionalUI>(instance, std::move(ui));
                    ownedplugin->pluginInstance()->SetUi(std::move(optionalui));
                }
                if(ownedplugin->pluginInstance()->Ui()->ui())
                {
                    ownedplugin->pluginInstance()->Ui()->ui()->SetShown(true);
                }
            }
            else
            {
                if(ownedplugin->pluginInstance() && ownedplugin->pluginInstance()->Ui() && ownedplugin->pluginInstance()->Ui()->ui())
                {
                    if(ownedplugin->pluginInstance()->Ui()->ui()->CanHide())
                    {
                        ownedplugin->pluginInstance()->Ui()->ui()->SetShown(false);
                    }
                    else
                    {
                        ownedplugin->pluginInstance()->SetUi({});
                    }
                }
            }
        }
        if(!Project().Reverb().ReverbLv2Uri().empty())
        {
            auto uri = Project().Reverb().ReverbLv2Uri();
            if(m_ReverbInstance && (m_ReverbInstance->Lv2Uri() != Project().Reverb().ReverbLv2Uri()))
            {
                pluginsToDiscard.push_back(std::move(m_ReverbInstance));
            }
            if(!m_ReverbInstance)
            {
                m_ReverbInstance = std::make_unique<PluginInstance>(std::move(uri), *jacksamplerate, m_RtProcessor);
            }
        }
        else
        {
            if(m_ReverbInstance)
            {
                pluginsToDiscard.push_back(std::move(m_ReverbInstance));
            }
        }
        if(m_ReverbInstance)
        {
            if(Project().Reverb().ShowGui())
            {
                if(!m_ReverbInstance->Ui())
                {
                    std::unique_ptr<lilvutils::UI> ui;
                    try
                    {
                        ui = std::make_unique<lilvutils::UI>(m_ReverbInstance->Instance());
                        m_ReverbUiClosedSink = std::make_unique<utils::NotifySink>(ui->OnClose(), [this](){
                            if(Project().Reverb().ShowGui())
                            {
                                auto newreverb = Project().Reverb().ChangeShowGui(false);
                                auto newproject = Project().ChangeReverb(std::move(newreverb));
                                SetProject(std::move(newproject));
                            }
                        });
                    }
                    catch(std::exception &e)
                    {
                        std::cerr << "Failed to create UI for reverb plugin " << m_ReverbInstance->Instance().plugin().Lv2Uri() << ": " << e.what() << std::endl;
                    }
                    auto optionaluiforreverb = std::make_unique<OptionalUI>(m_ReverbInstance->Instance(), std::move(ui));
                    m_ReverbInstance->SetUi(std::move(optionaluiforreverb));
                }
                if(m_ReverbInstance->Ui()->ui())
                {
                    m_ReverbInstance->Ui()->ui()->SetShown(true);
                }
            }
            else
            {
                if(m_ReverbInstance->Ui())
                {
                    if(m_ReverbInstance->Ui()->ui() && m_ReverbInstance->Ui()->ui()->CanHide())
                    {
                        m_ReverbInstance->Ui()->ui()->SetShown(false);
                    }
                    else
                    {
                        m_ReverbInstance->SetUi({});
                    }
                }
            }
        }
        m_OwnedPlugins = std::move(ownedPlugins);
        m_Parts = std::move(newparts);
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
                    if(presetindex < Project().Presets().size())
                    {
                        const auto &preset = Project().Presets()[presetindex];
                        if(preset)
                        {
                            auto instrumentindex = preset.value().InstrumentIndex();
                            auto pluginindex = m_Parts.at(partindex).PluginIndices().at(instrumentindex);
                            const auto &ownedplugin = m_OwnedPlugins.at(pluginindex);
                            if(ownedplugin)
                            {
                                std::string presetdir = PresetsDir() + "/" + preset->PresetSubDir();
                                auto &instance = ownedplugin->pluginInstance()->Instance();
                                ownedplugin->pluginInstance()->Instance().LoadState(presetdir);
                            }
                        }
                    }
                
                }
            }
        }
    }
    void Engine::ChangeReverbLv2Uri(std::string &&uri)
    {
        if(uri != Project().Reverb().ReverbLv2Uri())
        {
            auto oldpresetdir = Project().Reverb().ReverbPresetSubDir();
            auto reverb = Project().Reverb().ChangeReverbLv2Uri(std::move(uri)).ChangeReverbPresetSubDir(std::string());
            SetProject(Project().ChangeReverb(std::move(reverb)));
            SaveProjectSync();
            if(!oldpresetdir.empty())
            {
                auto dirToDelete = PresetsDir() + "/" + oldpresetdir;
                std::filesystem::remove_all(dirToDelete);
            }
        }
    }
    void Engine::StoreReverbPreset()
    {
        if(!m_ReverbInstance)
        {
            throw std::runtime_error("No reverb plugin loaded");
        }
        auto presetDir = SavePresetForInstance(m_ReverbInstance->Instance());
        auto dirToDelete = presetDir;
        utils::finally fin1([&](){
            if(!dirToDelete.empty()) std::filesystem::remove_all(dirToDelete);
        });
        auto relativePresetDir = std::filesystem::relative(presetDir, PresetsDir());
        auto oldpresetdir = Project().Reverb().ReverbPresetSubDir();
        auto reverb = Project().Reverb().ChangeReverbPresetSubDir(std::move(relativePresetDir));
        SetProject(Project().ChangeReverb(std::move(reverb)));
        SaveProjectSync();
        dirToDelete.clear();
        if(!oldpresetdir.empty())
        {
            dirToDelete = PresetsDir() + "/" + oldpresetdir;
        }
    }
    void Engine::LoadReverbPreset()
    {
        if(m_ReverbInstance)
        {
            auto presetSubdir = Project().Reverb().ReverbPresetSubDir();
            if(!presetSubdir.empty())
            {
                std::string presetdir = PresetsDir() + "/" + presetSubdir;
                m_ReverbInstance->Instance().LoadState(presetdir);
            }
        }
    }
    void Engine::DeletePreset(size_t presetindex)
    {
        if(presetindex < Project().Presets().size())
        {
            auto preset = Project().Presets()[presetindex];
            if(preset)
            {
                auto presetSubdir = preset->PresetSubDir();
                auto newproject = Project().ChangePreset(presetindex, {});
                for(size_t partindex = 0; partindex < Project().Parts().size(); ++partindex)
                {
                    auto &part = newproject.Parts()[partindex];
                    if(part.ActivePresetIndex() && (*part.ActivePresetIndex() == presetindex))
                    {
                        auto newpart = part.ChangeActivePresetIndex(std::nullopt);
                        newproject = newproject.ChangePart(partindex, std::move(newpart));
                    }
                }
                SetProject(std::move(newproject));
                SaveProjectSync();
                if(!presetSubdir.empty())
                {
                    auto dirToDelete = PresetsDir() + "/" + presetSubdir;
                    std::filesystem::remove_all(dirToDelete);
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
                    auto presetDir = SavePresetForInstance(ownedplugin->pluginInstance()->Instance());
                    auto dirToDelete = presetDir;
                    utils::finally fin1([&](){
                        if(!dirToDelete.empty()) std::filesystem::remove_all(dirToDelete);
                    });
                    auto relativePresetDir = std::filesystem::relative(presetDir, PresetsDir());

                    auto newproject = Project();
                    auto presets = newproject.Presets();
                    std::string oldpresetdir;
                    if(presetindex >= presets.size())
                    {
                        presets.resize(presetindex + 1);
                    }
                    else
                    {
                        if(presets[presetindex])
                        {
                            oldpresetdir = presets[presetindex]->PresetSubDir();
                        }
                    }
                    presets[presetindex] = project::TPreset(instrindex, std::string(name), std::move(relativePresetDir));
                    newproject.SetPresets(std::move(presets));
                    auto newpart = newproject.Parts().at(partindex).ChangeActivePresetIndex(presetindex).ChangeActiveInstrumentIndex(instrindex);
                    newproject = newproject.ChangePart(partindex, std::move(newpart));
                    SetProject(std::move(newproject));
                    SaveProjectSync();
                    dirToDelete.clear();
                    if(!oldpresetdir.empty())
                    {
                        dirToDelete = PresetsDir() + "/" + oldpresetdir;
                    }
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
        m_ProjectSaveThread = std::thread([this](){
            while(true)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                bool quitting = DoProjectSaveThread();
                if(quitting)
                {
                    break;
                }
            }
        });
    }
    bool Engine::DoProjectSaveThread()
    {
        std::unique_lock<std::mutex> lock1(m_SaveProjectNowMutex);
        bool quitting = false;
        std::unique_ptr<project::TProject> projectToSave;
        {
            std::unique_lock<std::mutex> lock(m_ProjectSaveMutex);
            quitting = m_Quitting;
            projectToSave = std::move(m_ProjectToSave);
        }
        if(projectToSave)
        {
            SaveProject(*projectToSave);
        }
        return quitting;
    }
    void Engine::LoadProject()
    {
        std::string projectfile = ProjectFile();
        if(!std::filesystem::exists(projectfile))
        {
            auto prj = project::TProject();
            ProjectToFile(prj, projectfile);
        }
        {
            auto prj = project::ProjectFromFile(projectfile);
            prj = prj.Change(0,false);
            SetProject(std::move(prj));
        }
        LoadReverbPreset();
    }
    void Engine::AddInstrument(const std::string &uri, bool shared)
    {
        std::string name;
        {
            auto uricopy = uri;
            auto plugin = lilvutils::Plugin(lilvutils::Uri(std::move(uricopy)));
            name = plugin.Name();
        }
        auto uricopy = uri;
        project::TInstrument newinstrument(std::move(uricopy), shared, std::move(name));
        auto newproject = Project().AddInstrument(std::move(newinstrument));
        SetProject(std::move(newproject));
    }
    void Engine::DeleteInstrument(size_t instrumentindex)
    {
        std::vector<std::string> presetSubdirsToDelete;
        for(const auto &preset: Project().Presets())
        {
            if(preset)
            {
                if(preset->InstrumentIndex() == instrumentindex)
                {
                    if(!preset->PresetSubDir().empty())
                    {
                        presetSubdirsToDelete.push_back(preset->PresetSubDir());
                    }
                }
            }
        }
        auto newproject = Project().DeleteInstrument(instrumentindex);
        SetProject(std::move(newproject));
        SaveProjectSync();
        for(const auto &presetSubdir: presetSubdirsToDelete)
        {
            auto dirToDelete = PresetsDir() + "/" + presetSubdir;
            std::filesystem::remove_all(dirToDelete);
        }
    }
    void Engine::SaveProjectSync()
    {
        DoProjectSaveThread();
    }
    void Engine::SaveProject(const project::TProject &project)
    {
        std::string projectfile = ProjectFile();
        std::string tempfile;
        do
        {
            tempfile = projectfile + ".tmp" + std::to_string(rand());
        } while (std::filesystem::exists(tempfile));
        ProjectToFile(project, tempfile);
        std::filesystem::rename(tempfile, projectfile);
    }
    Engine::~Engine()
    {
        {
            std::unique_lock<std::mutex> lock(m_ProjectSaveMutex);
            m_Quitting = true;
        }
        m_ProjectSaveThread.join();

        // this will deferredly delete the plugins and midi in ports that are no longer needed:
        SetProject(project::TProject());
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
    std::string Engine::ReverbPluginName() const
    {
        if(m_ReverbInstance)
        {
            return m_ReverbInstance->Instance().plugin().Name();
        }
        return {};
    }
    void Engine::SetProject(project::TProject &&project)
    {
        if(!m_Quitting)
        {
            std::unique_lock<std::mutex> lock(m_ProjectSaveMutex);
            m_ProjectToSave = std::make_unique<project::TProject>(project);
        }
        m_Project = std::move(project);
        SyncPlugins();
        OnProjectChanged().Notify();
    }
    void Engine::SyncPlugins()
    {
        std::vector<std::unique_ptr<jackutils::Port>> midiInPortsToDiscard;
        std::vector<std::unique_ptr<PluginInstance>> pluginsToDiscard;
        SyncPlugins(midiInPortsToDiscard, pluginsToDiscard);
        std::vector<std::unique_ptr<TAuxInPortLink>> newAuxInPorts, auxInPortsToDiscard;
        for(auto &auxport: m_AuxInPorts)
        {
            if(auxport->IsAttached())
            {
                newAuxInPorts.push_back(std::move(auxport));
            }
            else
            {
                auxInPortsToDiscard.push_back(std::move(auxport));
            }
        }
        m_AuxInPorts = std::move(newAuxInPorts);
        std::vector<std::unique_ptr<TAuxOutPortLink>> newAuxOutPorts, auxOutPortsToDiscard;
        for(auto &auxport: m_AuxOutPorts)
        {
            if(auxport->IsAttached())
            {
                newAuxOutPorts.push_back(std::move(auxport));
            }
            else
            {
                auxOutPortsToDiscard.push_back(std::move(auxport));
            }
        }
        m_AuxOutPorts = std::move(newAuxOutPorts);
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
        for(auto &port: auxInPortsToDiscard)
        {
            auto ptr = port.release();
            m_RtProcessor.DeferredExecuteAfterRoundTrip([ptr](){
                delete ptr;
            });  
        }
        for(auto &port: auxOutPortsToDiscard)
        {
            auto ptr = port.release();
            m_RtProcessor.DeferredExecuteAfterRoundTrip([ptr](){
                delete ptr;
            });  
        }
    }
    void Engine::ProcessMessages()
    {
        m_RtProcessor.ProcessMessagesInMainThread();
        ApplyJackConnections(false);
        for(const auto &plugin: OwnedPlugins())
        {
            if(plugin->pluginInstance() && plugin->pluginInstance()->Ui() && plugin->pluginInstance()->Ui()->ui())
            {
                plugin->pluginInstance()->Ui()->ui()->CallIdle();
            }
        }
        if(m_ReverbInstance && m_ReverbInstance->Ui() && m_ReverbInstance->Ui()->ui())
        {
            m_ReverbInstance->Ui()->ui()->CallIdle();
        }
        // if(m_NeedCloseUi)
        // {
        //     m_NeedCloseUi = false;
        //     if(Project().ShowUi())
        //     {
        //         SetProject(Project().Change(Project().FocusedPart(), false));
        //     }
        // }
        // if(m_NeedCloseReverbUi)
        // {
        //     m_NeedCloseReverbUi = false;
        //     if(Project().Reverb().ShowGui())
        //     {
        //         auto reverb = Project().Reverb().ChangeShowGui(false);
        //         SetProject(Project().ChangeReverb(std::move(reverb)));
        //     }
        // }
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
    void Engine::AuxInPortAdded(TAuxInPortBase *port)
    {
        m_AuxInPorts.push_back(std::make_unique<TAuxInPortLink>(port));
        SyncPlugins();
    }
    void Engine::AuxInPortRemoved(TAuxInPortBase *port)
    {
        for(auto &auxinport: m_AuxInPorts)
        {
            if(auxinport->AuxInPort() == port)
            {
                auxinport->Detach();
                // the port will be deleted asynchronuously
            }
        }
        SyncPlugins();
    }
    void Engine::AuxOutPortAdded(TAuxOutPortBase *port)
    {
        m_AuxOutPorts.push_back(std::make_unique<TAuxOutPortLink>(port, *this));
        SyncPlugins();
    }
    void Engine::AuxOutPortRemoved(TAuxOutPortBase *port)
    {
        for(auto &auxOutport: m_AuxOutPorts)
        {
            if(auxOutport->AuxOutPort() == port)
            {
                auxOutport->Detach();
                // the port will be deleted asynchronuously
            }
        }
        SyncPlugins();
    }
    void Engine::SendMidiAsync(const midi::TMidiOrSysexEvent &event, jackutils::Port &port)
    {
        auto span = event.Span();
        m_RtProcessor.SendMidiFromMainThread(span.data(), span.size(), port.get());
    }

    TAuxOutPortBase::TAuxOutPortBase(Engine& engine, std::string &&name) : m_Engine(&engine), m_Name(std::move(name))
    {
        m_Engine->AuxOutPortAdded(this);
    }
    TAuxOutPortBase::~TAuxOutPortBase()
    {   
        if(m_Engine)
        {
            m_Engine->AuxOutPortRemoved(this);
        }
    }

    TAuxOutPortLink::TAuxOutPortLink(TAuxOutPortBase *outport, Engine &engine) : m_AuxOutPort(outport), m_Port(std::string(outport->Name()), jackutils::Port::Kind::Midi, jackutils::Port::Direction::Output), m_Engine(engine)
    {
        outport->m_MidiSendFunc = [this](const midi::TMidiOrSysexEvent &event){
            m_Engine.SendMidiAsync(event, m_Port);
        };
    }

    void TAuxOutPortBase::Send(const midi::TMidiOrSysexEvent &event) const
    {
        if(m_MidiSendFunc)
        {
            m_MidiSendFunc(event);        
        }
    }

    TAuxInPortBase::TAuxInPortBase(Engine& engine, std::string &&name) : m_Engine(&engine), m_Name(std::move(name))
    {
        m_Engine->AuxInPortAdded(this);
    }
    TAuxInPortBase::~TAuxInPortBase()
    {   
        if(m_Engine)
        {
            m_Engine->AuxInPortRemoved(this);
        }
    }


    void TController::TInPort::OnMidi(const midi::TMidiOrSysexEvent &event)
    {
        m_Controller.OnMidiIn(event);
    }

    void TController::SendMidi(const midi::TMidiOrSysexEvent &event) const
    {
        m_OutPort.Send(event);
    }

    void TController::ProjectChanged()
    {
        OnProjectChanged(m_LastProject);
        m_LastProject = Project();
    }

}