#include "project.h"

namespace project
{
    TProject TestProject()
    {
        std::vector<TInstrument> instruments;
        std::vector<TPart> parts;
        parts.emplace_back("Upper", 0, std::nullopt, std::nullopt, std::vector<std::optional<size_t>>{}, 1.0f);
        parts.emplace_back("Lower", 1, std::nullopt, std::nullopt, std::vector<std::optional<size_t>>{}, 1.0f);
        std::vector<std::optional<TPreset>> presets;
        project::TReverb reverb;
        return TProject(std::move(instruments), std::move(parts), std::move(presets), std::move(reverb));
    }

    Json::Value ToJson(const TInstrument::TParameter &parameter)
    {
        Json::Value result;
        result["label"] = parameter.Label();
        result["initialvalue"] = parameter.InitialValue();
        result["ccnum"] = parameter.ControllerNumber();
        return result;
    }

    TInstrument::TParameter InstrumentParameterFromJson(const Json::Value &v)
    {
        return TInstrument::TParameter(v["ccnum"].asInt(), v["initialvalue"].asInt(), v["label"].asString());
    }

    Json::Value ToJson(const TInstrument &instrument)
    {
        Json::Value result;
        result["lv2uri"] = instrument.Lv2Uri();
        result["shared"] = instrument.Shared();
        result["name"] = instrument.Name();
        result["parameters"] = Json::Value(Json::arrayValue);
        for (const auto &parameter : instrument.Parameters())
        {
            result["parameters"].append(ToJson(parameter));
        }
        return result;
    }
    TInstrument InstrumentFromJson(const Json::Value &v)
    {
        std::vector<TInstrument::TParameter> parameters;
        for (const auto &parameter : v["parameters"])
        {
            parameters.push_back(InstrumentParameterFromJson(parameter));
        }
        return TInstrument(v["lv2uri"].asString(), v["shared"].asBool(), v["name"].asString(), std::move(parameters));
    }
    Json::Value ToJson(const TPart &part)
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
            result["presetindex"] = *part.ActivePresetIndex();
        }
        else
        {
            result["presetindex"] = Json::Value::null;
        }
        result["amplitudefactor"] = part.AmplitudeFactor();
        for (const auto &quickpreset : part.QuickPresets())
        {
            if(quickpreset)
            {
                result["quickpresets"].append(*quickpreset);
            }
            else
            {
                result["quickpresets"].append(Json::Value::null);
            }
        }

        return result;
    }
    TPart PartFromJson(const Json::Value &v)
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
        std::vector<std::optional<size_t>> quickPresets;
        for (const auto &preset : v["quickpresets"])
        {
            if(preset.isNull())
            {
                quickPresets.push_back(std::nullopt);
            }
            else
            {
                quickPresets.push_back(preset.asUInt64());
            }
        }
        return TPart(v["name"].asString(), v["midichannelforsharedinstruments"].asInt(), instrumentIndex, presetIndex, std::move(quickPresets), v["amplitudefactor"].asFloat());
    }
    Json::Value ToJson(const TPreset &preset)
    {
        Json::Value result;
        result["instrumentindex"] = preset.InstrumentIndex();
        result["name"] = preset.Name();
        result["subdir"] = preset.PresetSubDir();
        return result;
    }
    TPreset PresetFromJson(const Json::Value &v)
    {
        return TPreset(v["instrumentindex"].asInt(), v["name"].asString(), v["subdir"].asString());
    }
    Json::Value ToJson(const TProject &project)
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
        for (const auto &preset : project.Presets())
        {
            if(preset)
            {
                result["presets"].append(ToJson(*preset));
            }
            else
            {
                result["presets"].append(Json::Value::null);
            }
        }
        result["reverb"] = ToJson(project.Reverb());
        return result;
    }
    TProject ProjectFromJson(const Json::Value &v)
    {
        std::vector<TInstrument> instruments;
        for (const auto &instrument : v["instruments"])
        {
            instruments.push_back(InstrumentFromJson(instrument));
        }
        std::vector<TPart> parts;
        for (const auto &part : v["parts"])
        {
            parts.push_back(PartFromJson(part));
        }
        std::vector<std::optional<TPreset>> presets;
        for (const auto &preset : v["presets"])
        {
            if(preset.isNull())
            {
                presets.push_back(std::nullopt);
            }
            else
            {
                presets.push_back(PresetFromJson(preset));
            }
        }
        TReverb reverb;
        if(!v["reverb"].isNull())
        {
            reverb = ReverbFromJson(v["reverb"]);
        }
        return TProject(std::move(instruments), std::move(parts), std::move(presets), std::move(reverb));
    }
    TProject ProjectFromFile(const std::string &filename)
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
    void ProjectToFile(const TProject &project, const std::string &filename)
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
        result["mixlevel"] = reverb.MixLevel();
        return result;
    }
    TReverb ReverbFromJson(const Json::Value &v)
    {
        return TReverb(v["presetdir"].asString(), v["pluginuri"].asString(), v["mixlevel"].asFloat());
    }

    TProject TProject::DeleteInstrument(size_t index) const
    {
        std::vector<std::optional<TPreset>> newPresets;
        for (const auto &preset : m_Presets)
        {
            if(preset)
            {
                if(preset->InstrumentIndex() == index)
                {
                    newPresets.push_back(std::nullopt);
                }
                else if(preset->InstrumentIndex() > index)
                {
                    newPresets.push_back(preset.value().ChangeInstrumentIndex(preset->InstrumentIndex() - 1));
                }
                else
                {
                    newPresets.push_back(*preset);
                }
            }
            else
            {
                newPresets.push_back(std::nullopt);
            }
        }
        std::vector<TPart> newparts;
        for (const auto &part : m_Parts)
        {
            newparts.push_back(part.ChangeActiveInstrumentIndex(std::nullopt).ChangeActivePresetIndex(std::nullopt));
        }
        std::vector<TInstrument> newInstruments;
        for (size_t i = 0; i < m_Instruments.size(); i++)
        {
            if(i != index)
            {
                newInstruments.push_back(m_Instruments[i]);
            }
        }
        auto result = *this;
        result.m_Instruments = std::move(newInstruments);
        result.m_Parts = std::move(newparts);
        result.m_Presets = std::move(newPresets);
        return result;
    }

}