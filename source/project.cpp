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
        return Project(std::move(instruments), std::move(parts), std::move(quickPresets), std::nullopt, false);
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
            if(v.isNull())
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
        return Project(std::move(instruments), std::move(parts), std::move(quickPresets), focusedPart, v["showui"].asBool());
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
        for (const auto &audioOutput : jackConnections.AudioOutputs())
        {
            result["audiooutputs"].append(audioOutput);
        }
        for (const auto &midiInput : jackConnections.MidiInputs())
        {
            result["midiinputs"].append(midiInput);
        }
        return result;
    }
    TJackConnections JackConnectionsFromJson(const Json::Value &v)
    {
        std::array<std::string, 2> audioOutputs;
        std::vector<std::string> midiInputs;
        for (const auto &midiinput : v["midiinputs"])
        {
            midiInputs.push_back(midiinput.asString());
        }
        size_t index = 0;
        for (const auto &midiinput : v["audiooutputs"])
        {
            audioOutputs[index++] = midiinput.asString();
            if(index == 2) break;
        }
        return TJackConnections(std::move(audioOutputs), std::move(midiInputs));
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

}