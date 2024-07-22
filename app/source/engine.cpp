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

    void Engine::SendControllersForPart(size_t partindex, std::optional<size_t> controllerindexOrNull)
    {
        if(partindex < m_Data.Part2ControllerValues().size())
        {
            const auto& controllervalues = m_Data.Part2ControllerValues()[partindex];
            const auto &part = m_Data.Project().Parts().at(partindex);
            if(part.ActiveInstrumentIndex())
            {
                const auto &instrument = m_Data.Project().Instruments().at(*part.ActiveInstrumentIndex());
                size_t paramstart = 0;
                size_t paramend = instrument.Parameters().size();
                if(controllerindexOrNull)
                {
                    paramstart = std::max(paramstart, *controllerindexOrNull);
                    paramend = std::min(paramend, *controllerindexOrNull + 1);
                }
                paramend = std::min(paramend, controllervalues.size());
                for(size_t paramindex = paramstart; paramindex < paramend; ++paramindex)
                {
                    auto &param = instrument.Parameters().at(paramindex);
                    if(controllervalues[paramindex])
                    {
                        SendMidiToPart(midi::SimpleEvent::ControlChange(0, param.ControllerNumber(), *controllervalues[paramindex]), partindex);
                    }
                }
            }
        }
    }

    void Engine::SetData(TData &&data)
    {
        auto olddata = std::move(m_Data);
        m_Data = std::move(data);
        for(size_t partindex = 0; partindex < m_Data.Project().Parts().size(); ++partindex)
        {
            bool sendControllers = false;
            if(m_Data.Project().Parts()[partindex].ActivePresetIndex())
            {
                bool doload = false;
                if(partindex < olddata.Project().Parts().size())
                {
                    if(olddata.Project().Parts()[partindex].ActivePresetIndex() != m_Data.Project().Parts()[partindex].ActivePresetIndex())
                    {
                        doload = true;
                    }
                }
                else
                {
                    doload = true;
                }
                if(doload)
                {
                    sendControllers = true;
                    SyncPlugins();
                    LoadPresetForPart(partindex);
                }
            }
            std::optional<size_t> prevInstrumentIndex;
            if(partindex < olddata.Project().Parts().size())
            {
                prevInstrumentIndex = olddata.Project().Parts()[partindex].ActiveInstrumentIndex();
            }
            auto newInstrumentIndex = m_Data.Project().Parts()[partindex].ActiveInstrumentIndex();
            if(newInstrumentIndex && (prevInstrumentIndex != newInstrumentIndex))
            {
                sendControllers = true;
                SendMidiToPartInstrument(midi::SimpleEvent::AllNotesOff(0), partindex, *newInstrumentIndex);
                SendMidiToPartInstrument(midi::SimpleEvent::ControlChange(0, midi::ccSustainPedal, 0), partindex, *newInstrumentIndex);
            }
            if(sendControllers)
            {
                SendControllersForPart(partindex, std::nullopt);
            }
            else
            {
                if(partindex < m_Data.Part2ControllerValues().size())
                {
                    const auto& controllervalues = m_Data.Part2ControllerValues()[partindex];
                    for(size_t i =0; i < controllervalues.size(); i++)
                    {
                        std::optional<int> oldvalue;
                        if(partindex < olddata.Part2ControllerValues().size())
                        {
                            if(i < olddata.Part2ControllerValues()[partindex].size())
                            {
                                oldvalue = olddata.Part2ControllerValues()[partindex][i];
                            }
                        }
                        if(controllervalues[i] && (controllervalues[i] != oldvalue))
                        {
                            SendControllersForPart(partindex, i);
                        }
                    }
                }
            }
        }
        if(olddata.Project() != m_Data.Project())
        {
            if(!m_Quitting)
            {
                std::unique_lock<std::mutex> lock(m_ProjectSaveMutex);
                m_ProjectToSave = std::make_unique<project::TProject>(m_Data.Project());
            }
        }
        if( (olddata.Project() != m_Data.Project())
          || (olddata.ShowUi()  != m_Data.ShowUi())
            || (olddata.ShowReverbUi() != m_Data.ShowReverbUi()) 
            || (olddata.GuiFocusedPart() != m_Data.GuiFocusedPart()) )
        {
            SyncPlugins();
        }
        if(olddata.HammondData() != m_Data.HammondData())
        {
            UpdateHammondPlugins(olddata.HammondData(), false);
        }
        OnDataChanged().Notify();
    }
    void Engine::UpdateHammondPlugins(const project::THammondData &olddata, bool forceNow)
    {
        for(const auto &ownedplugin: m_OwnedPlugins)
        {
            if(ownedplugin && ownedplugin->pluginInstance())
            {
                if(m_ProcessingDataFromPlugin.count(ownedplugin->pluginInstance().get()) == 0)
                {
                    for(int channel = 0; channel < 2; channel++)
                    {
                        for(int drawbarindex = 0; drawbarindex < 9; drawbarindex++)
                        {
                            auto oldvalue = olddata.Part(channel).Registers()[drawbarindex];
                            auto newvalue = Data().HammondData().Part(channel).Registers()[drawbarindex];
                            if(forceNow || (oldvalue != newvalue))
                            {
                                auto controllervalue = (8 - std::clamp(newvalue, 0, 8)) * 16;
                                if(controllervalue > 64)
                                {
                                    controllervalue--;
                                }
                                SendMidi(midi::SimpleEvent::ControlChange(channel, 70 + drawbarindex, controllervalue), *ownedplugin->pluginInstance());
                            }
                        }
                    }
                    if(forceNow || (olddata.Percussion() != Data().HammondData().Percussion()))
                    {
                        SendMidi(midi::SimpleEvent::ControlChange(0, 80, Data().HammondData().Percussion()? 127 : 0), *ownedplugin->pluginInstance());
                    }
                    if(forceNow || (olddata.PercussionSoft() != Data().HammondData().PercussionSoft()))
                    {
                        SendMidi(midi::SimpleEvent::ControlChange(0, 81, Data().HammondData().PercussionSoft()? 127 : 0), *ownedplugin->pluginInstance());
                    }
                    if(forceNow || (olddata.PercussionFast() != Data().HammondData().PercussionFast()))
                    {
                        SendMidi(midi::SimpleEvent::ControlChange(0, 82, Data().HammondData().PercussionFast()? 127 : 0), *ownedplugin->pluginInstance());
                    }
                    if(forceNow || (olddata.Percussion2ndHarmonic() != Data().HammondData().Percussion2ndHarmonic()))
                    {
                        SendMidi(midi::SimpleEvent::ControlChange(0, 83, Data().HammondData().Percussion2ndHarmonic()? 127 : 0), *ownedplugin->pluginInstance());
                    }
                }
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
            part.MidiInPort()->LinkToAnyPortByPattern(midiInPortNames);
            partindex++;
        }
        for(size_t portindex = 0; portindex < m_AudioOutPorts.size(); ++portindex)
        {
            if(portindex < m_JackConnections.AudioOutputs().size())
            {
                const auto &audioportnames = m_JackConnections.AudioOutputs()[portindex];
                m_AudioOutPorts[portindex]->LinkToAllPortsByPattern(audioportnames);
            }
        }
        if(m_VocoderInPort)
        {
            m_VocoderInPort->LinkToAnyPortByPattern(m_JackConnections.VocoderInput());
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
                        auxinport->Port().LinkToAnyPortByPattern(portpair.second);
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
                        auxOutport->Port().LinkToAllPortsByPattern(portpair.second);
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
            const auto &instrument = Project().Instruments().at(ownedplugin->OwningInstrumentIndex());
            bool isActive = activePluginIndices.find(ownedPluginIndex) != activePluginIndices.end();
            for(const auto &presetLoader: m_PresetLoaders)
            {
                if(&presetLoader->PluginInstance() == ownedplugin.get())
                {
                    isActive = false;
                }
            }
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
                int transpose = 0;
                auto midiInBuf = ownedplugin->pluginInstance()->GetMidiInBuf();
                ownedPluginIndex2RtPluginIndex.push_back(plugins.size());
                plugins.emplace_back(&ownedplugin->pluginInstance()->Instance(), amplitude, doOverridePort, midiInBuf, transpose, instrument.HasVocoderInput());
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
        jack_port_t* vocoderInPort = m_VocoderInPort->get();
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
        float levelmeter_flowpass = 50.0f; // hz
        float levelMeterTimeConstant = std::exp(-2.0f * 3.14159265358979323846f * levelmeter_flowpass / 48000.0f);
        return realtimethread::Data(std::move(plugins), std::move(midiPorts), std::move(auxInPorts), std::move(auxOutPorts), std::move(outputAudioPorts), reverbinstance, reverblevel, vocoderInPort, levelMeterTimeConstant);
    }

    void Engine::OnMidiFromPlugin(PluginInstanceForPart *sender, const midi::TMidiOrSysexEvent &evt)
    {
        if(sender->pluginInstance())
        {
            if( (sender->OwningInstrumentIndex() < Project().Instruments().size()) && (Project().Instruments().at(sender->OwningInstrumentIndex()).IsHammond()))
            {
                if(m_ProcessingDataFromPlugin.count(sender->pluginInstance().get()) == 0)
                {
                    m_ProcessingDataFromPlugin.insert(sender->pluginInstance().get());
                    utils::finally fin1([&](){
                        m_ProcessingDataFromPlugin.erase(sender->pluginInstance().get());
                    });
                    if(!evt.IsSysex())
                    {
                        auto simpleevent = evt.GetSimpleEvent();
                        if(simpleevent.type() == midi::SimpleEvent::Type::ControlChange)
                        {
                            if(simpleevent.ControlNumber() >= 70 && simpleevent.ControlNumber() < 79)
                            {
                                if(simpleevent.Channel() < 2)
                                {
                                    size_t partindex = (size_t)simpleevent.Channel();
                                    auto drawbarindex = (size_t)(simpleevent.ControlNumber() - 70);
                                    int drawbarvalue = std::clamp(8 - ((simpleevent.ControlValue() + 8) / 16), 0, 8);
                                    auto oldvalue = Data().HammondData().Part(partindex).Registers().at(drawbarindex);
                                    if(drawbarvalue != oldvalue)
                                    {
                                        auto newdata = Data().ChangeHammondData(Data().HammondData().ChangePart(partindex, Data().HammondData().Part(partindex).ChangeRegister(drawbarindex, drawbarvalue)));
                                        SetData(std::move(newdata));
                                    }
                                }
                            }
                            else if( (simpleevent.ControlNumber() >= 80) && (simpleevent.ControlNumber() < 84) && (simpleevent.Channel() == 0))
                            {
                                bool boolvalue = simpleevent.ControlValue() > 63;
                                auto hammonddata = Data().HammondData();
                                if(simpleevent.ControlNumber() == 80)
                                {
                                    hammonddata = hammonddata.ChangePercussion(boolvalue);
                                }
                                else if(simpleevent.ControlNumber() == 81)
                                {
                                    hammonddata = hammonddata.ChangePercussionSoft(boolvalue);
                                }
                                else if(simpleevent.ControlNumber() == 82)
                                {
                                    hammonddata = hammonddata.ChangePercussionFast(boolvalue);
                                }
                                else if(simpleevent.ControlNumber() == 83)
                                {
                                    hammonddata = hammonddata.ChangePercussion2ndHarmonic(boolvalue);
                                }
                                auto newdata = Data().ChangeHammondData(std::move(hammonddata));
                                SetData(std::move(newdata));
                            }
                        }
                    }
                }
            }
        }
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
                if(!instrument.IsHammond())
                {
                    owningpart = partindex;
                }
                if(instrument.IsHammond() && (partindex > 0))
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
                        plugin_uq = std::make_unique<PluginInstanceForPart>(std::move(lv2uri), *jacksamplerate, owningpart, instrumentindex, m_RtProcessor, [this](PluginInstanceForPart *sender, const midi::TMidiOrSysexEvent &evt){
                            OnMidiFromPlugin(sender, evt);
                        });
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
                m_Plugin2LoadQueue.erase(plugin.get());
                m_OwnedPluginsToBeDiscardedAfterLoad.push_back(std::move(plugin));
            }
        }

        decltype(m_OwnedPluginsToBeDiscardedAfterLoad) newOwnedPluginsToBeDiscardedAfterLoad;
        for(auto &plugin: m_OwnedPluginsToBeDiscardedAfterLoad)
        {
            if(plugin)
            {
                if(!IsPluginLoading(plugin.get()))
                {
                    // not being loaded
                    pluginsToDiscard.push_back(std::move(plugin->pluginInstance()));
                    plugin = {};
                }
                else
                {
                    // still being loaded:
                    newOwnedPluginsToBeDiscardedAfterLoad.push_back(std::move(plugin));
                }
            }
        }
        m_OwnedPluginsToBeDiscardedAfterLoad = std::move(newOwnedPluginsToBeDiscardedAfterLoad);

        for(const auto &ownedplugin: ownedPlugins)
        {
            bool isfocused = false;
            if(Data().GuiFocusedPart())
            {
                if( (ownedplugin->OwningPart() && (*Data().GuiFocusedPart() == *ownedplugin->OwningPart()))
                    || (!ownedplugin->OwningPart()))
                {
                    if(Project().Parts().at(*Data().GuiFocusedPart()).ActiveInstrumentIndex() == ownedplugin->OwningInstrumentIndex())
                    {
                        isfocused = true;
                    }
                }
            }
            bool showgui = isfocused && Data().ShowUi();
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
                            if(Data().ShowUi())
                            {
                                auto newdata = Data().ChangeShowUi(false);
                                SetData(std::move(newdata));
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
                m_ReverbInstance = std::make_unique<PluginInstance>(std::move(uri), *jacksamplerate, m_RtProcessor, lilvutils::Instance::TMidiCallback());
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
            if(Data().ShowReverbUi())
            {
                if(!m_ReverbInstance->Ui())
                {
                    std::unique_ptr<lilvutils::UI> ui;
                    try
                    {
                        ui = std::make_unique<lilvutils::UI>(m_ReverbInstance->Instance());
                        m_ReverbUiClosedSink = std::make_unique<utils::NotifySink>(ui->OnClose(), [this](){
                            if(Data().ShowReverbUi())
                            {
                                auto newdata = Data().ChangeShowReverbUi(false);
                                SetData(std::move(newdata));
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
    void Engine::LoadPresetForPart(size_t partindex)
    {
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
                        if(Project().Instruments().at(instrumentindex).IsHammond())
                        {
                            UpdateHammondPlugins(Data().HammondData(), true);
                        }
                        else
                        {
                            const auto &ownedplugin = m_OwnedPlugins.at(pluginindex);
                            if(ownedplugin)
                            {
                                std::string presetdir = PresetsDir() + "/" + preset->PresetSubDir();
                                m_Plugin2LoadQueue[ownedplugin.get()] = presetdir;
                                StartLoading();
                                // ownedplugin->pluginInstance()->Instance().LoadState(presetdir);
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

    void Engine::SendMidiToPart(const midi::TMidiOrSysexEvent &event, size_t partindex)
    {
        if(partindex < Project().Parts().size())
        {
            const auto &part = Project().Parts().at(partindex);
            if(part.ActiveInstrumentIndex())
            {
                SendMidiToPartInstrument(event, partindex, *part.ActiveInstrumentIndex());
            }
        }
    }

    void Engine::SendMidiToPartInstrument(const midi::TMidiOrSysexEvent &event, size_t partindex, size_t instrumentindex)
    {
        if( (partindex < Project().Parts().size()) && (partindex < m_Parts.size()) )
        {
            const auto &part = Project().Parts().at(partindex);
            if(instrumentindex < m_Parts.at(partindex).PluginIndices().size())
            {
                auto ownedpluginindex = m_Parts[partindex].PluginIndices()[instrumentindex];
                const auto &ownedplugin = m_OwnedPlugins[ownedpluginindex];
                if(ownedplugin->pluginInstance())
                {
                    int midiChannel = 0;
                    if(!ownedplugin->OwningPart())
                    {
                        midiChannel = part.MidiChannelForSharedInstruments();
                    }
                    auto modifiedevent = event.ChangeChannel(midiChannel);
                    SendMidi(modifiedevent, *ownedplugin->pluginInstance());
                }
            }
        }
    }

    void Engine::SendMidi(const midi::TMidiOrSysexEvent &event, const PluginInstance &plugininstance)
    {
        if(plugininstance.Plugin().MidiInputIndex())
        {
            if(auto atomconnection = dynamic_cast<lilvutils::TConnection<lilvutils::TAtomPort>*>(plugininstance.Instance().Connections().at(*plugininstance.Plugin().MidiInputIndex()).get()))
            {
                auto midiInBuf = &atomconnection->BufferIterator();
                m_RtProcessor.SendMidiToPluginFromMainThread(event.Span().data(), event.Span().size(), midiInBuf);
            }
        }
    }

    void Engine::SaveCurrentPreset(size_t partindex, size_t presetindex, const std::string &name)
    {
        if( (partindex < Project().Parts().size()) && Project().Parts()[partindex].ActiveInstrumentIndex())
        {
            const auto &instrumentindex2ownedpluginindex = m_Parts[partindex].PluginIndices();
            auto instrindex = *Project().Parts()[partindex].ActiveInstrumentIndex();
            if( (instrindex >= 0) && (instrindex < instrumentindex2ownedpluginindex.size()) )
            {
                auto ownedpluginindex = instrumentindex2ownedpluginindex[instrindex];
                const auto &ownedplugin = m_OwnedPlugins[ownedpluginindex];
                if(ownedplugin)
                {
                    if(IsPluginLoading(ownedplugin.get()))
                    {
                        throw std::runtime_error("Cannot save, plugin is loading");
                    }
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
                    presets[presetindex] = project::TPreset(instrindex, std::string(name), std::move(relativePresetDir), std::nullopt);
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

    Engine::Engine(uint32_t maxBlockSize, int argc, char** argv, std::string &&projectdir, utils::TEventLoop &eventLoop) : m_BufferSize(64),m_JackClient {"JN Live", [this](jack_nframes_t nframes){
        m_RtProcessor.Process(nframes);
    }}, m_LilvWorld(m_JackClient.SampleRate(), maxBlockSize, argc, argv), m_ProjectDir(std::move(projectdir)), m_EventLoop(eventLoop), m_CleanupPresetLoadersAction(m_EventLoop, [this](){CleanupPresetLoaders();})
    {
        {
            auto errcode = jack_set_buffer_size(jackutils::Client::Static().get(), BufferSize()
            );
            if(errcode)
            {
                throw std::runtime_error("jack_set_buffer_size failed");
            }
        }
        m_AudioOutPorts.push_back(std::make_unique<jackutils::Port>("out_l", jackutils::Port::Kind::Audio, jackutils::Port::Direction::Output));
        m_AudioOutPorts.push_back(std::make_unique<jackutils::Port>("out_r", jackutils::Port::Kind::Audio, jackutils::Port::Direction::Output));
        if(!std::filesystem::exists(m_ProjectDir))
        {
            std::filesystem::create_directory(m_ProjectDir);
        }
        m_VocoderInPort = std::make_unique<jackutils::Port>("vocoder_in", jackutils::Port::Kind::Audio, jackutils::Port::Direction::Input);
        LoadProject();
        LoadJackConnections();
        SyncPlugins();
        ApplyJackConnections(true);

        {
            auto errcode = jack_activate(jackutils::Client::Static().get());
            if(errcode)
            {
                throw std::runtime_error("jack_activate failed");
            }
        }

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
        LoadFirstHammondPreset();
    }
    void Engine::LoadJackConnections()
    {
        std::string jackconnectionfile = ProjectDir() + "/jackconnection.json";
        if(!std::filesystem::exists(jackconnectionfile))
        {
            std::array<std::vector<std::string>, 2> m_AudioOutputs;
            std::vector<std::vector<std::string>> midiInputs {};
            std::vector<std::pair<std::string, std::vector<std::string>>> controllermidiports;
            std::vector<std::string> vocoderinput;
            project::TJackConnections jackconn(std::move(m_AudioOutputs), std::move(midiInputs), std::move(controllermidiports), std::move(vocoderinput));
            JackConnectionsToFile(jackconn, jackconnectionfile);
        }
        {
            m_JackConnections = project::JackConnectionsFromFile(jackconnectionfile);
        }
    }      
    void Engine::LoadFirstHammondPreset()
    {
        for(const auto &preset: Project().Presets())
        {
            if(preset)
            {
                if(Project().Instruments().at(preset->InstrumentIndex()).IsHammond())
                {
                    auto instrumentindex = preset.value().InstrumentIndex();
                    auto pluginindex = m_Parts.at(0).PluginIndices().at(instrumentindex);
                    const auto &ownedplugin = m_OwnedPlugins.at(pluginindex);
                    if(ownedplugin)
                    {
                        std::string presetdir = PresetsDir() + "/" + preset->PresetSubDir();
                        auto &instance = ownedplugin->pluginInstance()->Instance();
                        ownedplugin->pluginInstance()->Instance().LoadState(presetdir);
                    }
                    break;
                }
            }
        }
        for(const auto &ownedplugin: m_OwnedPlugins)
        {
            if(ownedplugin && Project().Instruments().at(ownedplugin->OwningInstrumentIndex()).IsHammond())
            {
                LoadPresetForPart(*ownedplugin->OwningPart());
            }


        }

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
        bool hasVocoder = false;
        project::TInstrument newinstrument(std::move(uricopy), shared, std::move(name), {}, hasVocoder);
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

        m_Plugin2LoadQueue.clear();
        while(!m_PresetLoaders.empty())
        {
            m_EventLoop.ProcessPendingMessages();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        // this will deferredly delete the plugins and midi in ports that are no longer needed:
        auto newdata = Data().ChangeShowUi(false).ChangeShowReverbUi(false);
        SetData(std::move(newdata));
        SetData(TData());
        for(auto &port: m_AudioOutPorts)
        {
            auto ptr = port.release();
            m_RtProcessor.DeferredExecuteAfterRoundTrip([ptr](){
                delete ptr;
            });  
        }
        m_AudioOutPorts.clear();
        {
            auto ptr = m_VocoderInPort.release();
            m_RtProcessor.DeferredExecuteAfterRoundTrip([ptr](){
                delete ptr;
            });  
        }
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
        auto newdata = Data().ChangeProject(std::move(project));
        SetData(std::move(newdata));
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
    void Engine::PresetLoaderFinished(TPresetLoader *loader)
    {
        m_CleanupPresetLoadersAction.Signal();
    }
    void Engine::CleanupPresetLoaders()
    {
        std::vector<std::unique_ptr<TPresetLoader>> newPresetLoaders;
        bool changed = false;
        for(auto &loader: m_PresetLoaders)
        {
            if(loader->Finished())
            {
                if(loader->PluginInstance().OwningPart() && (Project().Parts().size() > *loader->PluginInstance().OwningPart()))
                {
                    if(loader->PluginInstance().OwningInstrumentIndex() && (loader->PluginInstance().OwningInstrumentIndex() == Project().Parts().at(*loader->PluginInstance().OwningPart()).ActiveInstrumentIndex()))
                    {
                        // we've finished loading the patch for the active instrument of a part
                        // Send controller values:
                        SendControllersForPart(*loader->PluginInstance().OwningPart(), std::nullopt);
                    }
                }


                changed = true;
            }
            else
            {
                newPresetLoaders.push_back(std::move(loader));
            }
        }
        m_PresetLoaders = std::move(newPresetLoaders);
        if(changed)
        {
            SyncPlugins();
            SyncRtData();
            StartLoading();
        }
    }

    bool Engine::IsPluginLoading(PluginInstanceForPart *plugin) const
    {
        return (std::find_if(m_PresetLoaders.begin(), m_PresetLoaders.end(), [&plugin](const std::unique_ptr<TPresetLoader> &presetLoader){
            return &presetLoader->PluginInstance() == plugin;
        }) != m_PresetLoaders.end());

    }
    void Engine::StartLoading()
    {
    again:
        for(auto it = m_Plugin2LoadQueue.begin(); it != m_Plugin2LoadQueue.end(); it++)
        {
            auto plugin = it->first;
            if(!IsPluginLoading(plugin))
            {
                auto presetdir = it->second;
                m_Plugin2LoadQueue.erase(it);
                auto loader_uq = std::make_unique<TPresetLoader>(*this, *plugin, presetdir);
                auto loader = loader_uq.get();
                m_PresetLoaders.push_back(std::move(loader_uq));
                SyncRtData();
                loader->Start();
                goto again; // because it is no longer valid
            }
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

    void TController::DataChanged()
    {
        OnDataChanged(m_LastData);
        m_LastData = Engine().Data();
    }

    TPresetLoader::TPresetLoader(Engine &engine, PluginInstanceForPart &pluginInstance, const std::string &presetdir) : m_Engine(engine), m_PluginInstance(pluginInstance), m_DidRoundTripAction(m_LoaderThread, [this](){StartLoad();}),  m_LoadDoneAction(m_Engine.EventLoop(), [this](){LoadDone();}), m_PresetDir(presetdir)
    {
    }

    void TPresetLoader::Start()
    {
        m_Engine.RtProcessor().DeferredExecuteAfterRoundTrip([this](){
            m_DidRoundTripAction.Signal();
        });  
        m_LoaderThread.Run();
    }

    void TPresetLoader::StartLoad()
    {
        // called from loader thread after we have made a round trip to the audio thread
        try
        {    
            m_PluginInstance.pluginInstance()->Instance().LoadState(m_PresetDir);

/*
            auto midiinbuf = m_PluginInstance.pluginInstance()->GetMidiInBuf();
            if(midiinbuf)
            {
                auto uridMidiEvent = lilvutils::World::Static().UriMapLookup(LV2_MIDI__MidiEvent);
                // send a midi event to the plugin and let it generate audio. Misbehaving plugins may initially cause an xrun
                auto pedalon = midi::SimpleEvent::ControlChange(0, 64, 127);
                //auto pedaloff = midi::SimpleEvent::NoteOn(0, 64, 100);

                lv2_evbuf_write(midiinbuf, 0, 0, uridMidiEvent, pedalon.Span().size(), pedalon.Span().data());

                for(int i=0; i < 1000; i++)
                {
                    m_PluginInstance.pluginInstance()->Instance().Run(m_Engine.BufferSize());
                }
                auto pedaloff = midi::SimpleEvent::ControlChange(0, 64, 0);
                lv2_evbuf_write(midiinbuf, 0, 0, uridMidiEvent, pedalon.Span().size(), pedaloff.Span().data());
            }
*/
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
        m_LoadDoneAction.Signal();
    }

    void TPresetLoader::LoadDone()
    {
        m_LoaderThread.Stop();
        m_Finished = true;
        m_Engine.PresetLoaderFinished(this);
    }

    LV2_Evbuf_Iterator* PluginInstance::GetMidiInBuf() const
    {
        LV2_Evbuf_Iterator *midiInBuf = nullptr;
        if(Plugin().MidiInputIndex())
        {
            if(auto atomconnection = dynamic_cast<lilvutils::TConnection<lilvutils::TAtomPort>*>(Instance().Connections().at(*Plugin().MidiInputIndex()).get()))
            {
                midiInBuf = &atomconnection->BufferIterator();
            }
        }
        return midiInBuf;
    }
}