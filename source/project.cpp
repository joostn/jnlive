#include "project.h"

namespace project
{
    Project TestProject()
    {
        std::vector<Instrument> instruments;
        instruments.emplace_back("http://gareus.org/oss/lv2/b_synth", true, "SetBfree");
        instruments.emplace_back("http://sfztools.github.io/sfizz", false, "SFizz");
        instruments.emplace_back("http://synthv1.sourceforge.net/lv2", false, "LV2");
        instruments.emplace_back("http://tytel.org/helm", false, "Helm");
        instruments.emplace_back("http://www.openavproductions.com/sorcer", false, "Sorcer");
        std::vector<Part> parts;
        parts.emplace_back("Upper", 0, std::nullopt, std::nullopt, 1.0f);
        parts.emplace_back("Lower", 1, std::nullopt, std::nullopt, 1.0f);
        std::vector<std::optional<TQuickPreset>> quickPresets;
        project::TReverb reverb;
        return Project(std::move(instruments), std::move(parts), std::move(quickPresets), std::nullopt, false, {});
    }

    Json::Value ToJson(const Instrument &instrument)
    {
        Json::Value result;
        result["lv2uri"] = instrument.Lv2Uri();
        result["shared"] = instrument.Shared();
        result["name"] = instrument.Name();
        return result;
    }
    Instrument InstrumentFromJson(const Json::Value &v)
    {
        return Instrument(v["lv2uri"].asString(), v["shared"].asBool(), v["name"].asString());
    }
    Json::Value ToJson(const Part &part)
    {
        Json::Value result;
        result["name"] = part.Name();
        result["midichannelforsharedinstruments"] = part.MidiChannelForSharedInstruments();
        if(part.ActiveInstrumentIndex())
        {
            result["instrumentindex"] = *part.ActiveInstrumentIndex();
        }
        else
        {
            result["instrumentindex"] = Json::Value::null;
        }
        if(part.ActivePresetIndex())
        {
            result["instrumentindex"] = *part.ActivePresetIndex();
        }
        else
        {
            result["presetindex"] = Json::Value::null;
        }
        result["amplitudefactor"] = part.AmplitudeFactor();
        return result;
    }
    Part PartFromJson(const Json::Value &v)
    {
        std::optional<size_t> instrumentIndex;
        if(!v["instrumentindex"].isNull())
        {
            instrumentIndex = v["instrumentindex"].asInt();
        }
        std::optional<size_t> presetIndex;
        if(!v["presetindex"].isNull())
        {
            presetIndex = v["presetindex"].asInt();
        }

        return Part(v["name"].asString(), v["midichannelforsharedinstruments"].asInt(), instrumentIndex, presetIndex, v["amplitudefactor"].asFloat());
    }
    Json::Value ToJson(const TQuickPreset &quickPreset)
    {
        Json::Value result;
        result["instrumentindex"] = quickPreset.InstrumentIndex();
        result["name"] = quickPreset.Name();
        result["subdir"] = quickPreset.PresetSubDir();
        return result;
    }
    TQuickPreset QuickPresetFromJson(const Json::Value &v)
    {
        return TQuickPreset(v["instrumentindex"].asInt(), v["name"].asString(), v["subdir"].asString());
    }
    Json::Value ToJson(const Project &project)
    {
        Json::Value result;
        for (const auto &instrument : project.Instruments())
        {
            result["instruments"].append(ToJson(instrument));
        }
        for (const auto &part : project.Parts())
        {
            result["parts"].append(ToJson(part));
        }
        for (const auto &quickPreset : project.QuickPresets())
        {
            if(quickPreset)
            {
                result["quickpresets"].append(ToJson(*quickPreset));
            }
            else
            {
                result["quickpresets"].append(Json::Value::null);
            }
        }
        result["showui"] = project.ShowUi();
        if(project.FocusedPart())
        {
            result["focusedpart"] = *project.FocusedPart();
        }
        else
        {
            result["focusedpart"] = Json::Value::null;
        }
        result["reverb"] = ToJson(project.Reverb());
        return result;
    }
    Project ProjectFromJson(const Json::Value &v)
    {
        std::vector<Instrument> instruments;
        for (const auto &instrument : v["instruments"])
        {
            instruments.push_back(InstrumentFromJson(instrument));
        }
        std::vector<Part> parts;
        for (const auto &part : v["parts"])
        {
            parts.push_back(PartFromJson(part));
        }
        std::vector<std::optional<TQuickPreset>> quickPresets;
        for (const auto &quickPreset : v["quickpresets"])
        {
            if(quickPreset.isNull())
            {
                quickPresets.push_back(std::nullopt);
            }
            else
            {
                quickPresets.push_back(QuickPresetFromJson(quickPreset));
            }
        }
        std::optional<size_t> focusedPart;
        if(v["focusedpart"].isNull())
        {
            focusedPart = std::nullopt;
        }
        else
        {
            focusedPart = v["focusedpart"].asInt();
        }
        TReverb reverb;
        if(!v["reverb"].isNull())
        {
            reverb = ReverbFromJson(v["reverb"]);
        }
        return Project(std::move(instruments), std::move(parts), std::move(quickPresets), focusedPart, v["showui"].asBool(), std::move(reverb));
    }
    Project ProjectFromFile(const std::string &filename)
    {
        Json::Value v;
        std::ifstream ifs(filename);
        if (!ifs)
        {
            throw std::runtime_error("Could not open file for reading: " + filename);
        }
        ifs >> v;
        return ProjectFromJson(v);
    }
    void ProjectToFile(const Project &project, const std::string &filename)
    {
        Json::Value v = ToJson(project);
        std::ofstream ofs(filename);
        if (!ofs)
        {
            throw std::runtime_error("Could not open file for writing: " + filename);
        }
        ofs << v;
    }

    Json::Value ToJson(const TJackConnections &jackConnections)
    {
        Json::Value result;
        result["audiooutputs"] = Json::Value(Json::arrayValue);
        for (const std::vector<std::string> &audioOutputs : jackConnections.AudioOutputs())
        {
            result["audiooutputs"].append(Json::Value(Json::arrayValue));
            for (const std::string &audioOutput : audioOutputs)
            {
                result["audiooutputs"][result["audiooutputs"].size() - 1].append(audioOutput);
            }
        }
        for (const auto &midiInputs : jackConnections.MidiInputs())
        {
            result["midiinputs"].append(Json::Value(Json::arrayValue));
            for (const std::string &midiInput : midiInputs)
            {
                result["midiinputs"][result["midiinputs"].size() - 1].append(midiInput);
            }
        }
        for(const auto &[id, portnames]: jackConnections.ControllerMidiPorts())
        {
            result["controllermidiports"].append(Json::Value(Json::objectValue));
            result["controllermidiports"][result["controllermidiports"].size() - 1]["id"] = id; 
            result["controllermidiports"][result["controllermidiports"].size() - 1]["device"] = Json::Value(Json::objectValue);
            for(const auto &portname: portnames)
            {
                result["controllermidiports"][result["controllermidiports"].size() - 1]["device"].append(portname);
            }
        }
        return result;
    }
    TJackConnections JackConnectionsFromJson(const Json::Value &v)
    {
        std::vector<std::vector<std::string>> midiInputs;
        for (const auto &midiinput : v["midiinputs"])
        {
            std::vector<std::string> portnames;
            for (const auto &midiinputport : midiinput)
            {
                portnames.push_back(midiinputport.asString());
            }
            midiInputs.push_back(std::move(portnames));
        }
        size_t index = 0;
        std::array<std::vector<std::string>, 2> audioOutputs;
        for (const auto &audiooutput : v["audiooutputs"])
        {
            std::vector<std::string> portnames;
            for (const auto &p : audiooutput)
            {
                portnames.push_back(p.asString());
            }
            audioOutputs[index++] = std::move(portnames);
            if(index == 2) break;
        }
        std::vector<std::pair<std::string /* id */, std::vector<std::string> /* jackname */>> controllermidiports;
        for(const auto &controllermidiport: v["controllermidiports"])
        {
            auto portid = controllermidiport["id"].asString();
            std::vector<std::string> portnames;
            for (const auto &p : controllermidiport["device"])
            {
                portnames.push_back(p.asString());
            }
            controllermidiports.emplace_back(portid, std::move(portnames));
        }
        return TJackConnections(std::move(audioOutputs), std::move(midiInputs), std::move(controllermidiports));
    }

    TJackConnections JackConnectionsFromFile(const std::string &filename)
    {
        Json::Value v;
        std::ifstream ifs(filename);
        if (!ifs)
        {
            throw std::runtime_error("Could not open file for reading: " + filename);
        }
        ifs >> v;
        return JackConnectionsFromJson(v);
    }
    void JackConnectionsToFile(const TJackConnections &jackConnections, const std::string &filename)
    {
        Json::Value v = ToJson(jackConnections);
        std::ofstream ofs(filename);
        if (!ofs)
        {
            throw std::runtime_error("Could not open file for writing: " + filename);
        }
        ofs << v;
    }
    Json::Value ToJson(const TReverb &reverb)
    {
        Json::Value result;
        result["presetdir"] = reverb.ReverbPresetSubDir();
        result["pluginuri"] = reverb.ReverbLv2Uri();
        result["showgui"] = reverb.ShowGui();
        result["mixlevel"] = reverb.MixLevel();
        return result;
    }
    TReverb ReverbFromJson(const Json::Value &v)
    {
        return TReverb(v["presetdir"].asString(), v["pluginuri"].asString(), v["mixlevel"].asFloat(), v["showgui"].asBool());
    }

    Project Project::DeleteInstrument(size_t index) const
    {
        std::vector<std::optional<TQuickPreset>> newQuickPresets;
        for (const auto &quickPreset : m_QuickPresets)
        {
            if(quickPreset)
            {
                if(quickPreset->InstrumentIndex() == index)
                {
                    newQuickPresets.push_back(std::nullopt);
                }
                else if(quickPreset->InstrumentIndex() > index)
                {
                    newQuickPresets.push_back(quickPreset.value().ChangeInstrumentIndex(quickPreset->InstrumentIndex() - 1));
                }
                else
                {
                    newQuickPresets.push_back(*quickPreset);
                }
            }
            else
            {
                newQuickPresets.push_back(std::nullopt);
            }
        }
        std::vector<Part> newparts;
        for (const auto &part : m_Parts)
        {
            newparts.push_back(part.ChangeActiveInstrumentIndex(std::nullopt).ChangeActivePresetIndex(std::nullopt));
        }
        std::vector<Instrument> newInstruments;
        for (size_t i = 0; i < m_Instruments.size(); i++)
        {
            if(i != index)
            {
                newInstruments.push_back(m_Instruments[i]);
            }
        }
        auto reverb = Reverb();
        return Project(std::move(newInstruments), std::move(newparts), std::move(newQuickPresets), FocusedPart(), ShowUi(), std::move(reverb));
    }

}