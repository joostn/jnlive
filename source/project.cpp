#include "project.h"

namespace project
{
    Project TestProject()
    {
        std::vector<Instrument> instruments;
        instruments.emplace_back("http://gareus.org/oss/lv2/b_synth", true);
        instruments.emplace_back("http://sfztools.github.io/sfizz", false);
        instruments.emplace_back("http://synthv1.sourceforge.net/lv2", false);
        instruments.emplace_back("http://tytel.org/helm", false);
        instruments.emplace_back("http://www.openavproductions.com/sorcer", false);
        std::vector<Part> parts;
        parts.emplace_back("Upper", "S61mk2", 1);
        parts.emplace_back("Lower", "S88mk2", 2);
        std::vector<TQuickPreset> quickPresets;
        quickPresets.emplace_back(0, 0);
        quickPresets.emplace_back(1, 0);
        quickPresets.emplace_back(2, 0);
        quickPresets.emplace_back(3, 0);
        quickPresets.emplace_back(4, 0);
        quickPresets.emplace_back(0, 1);
        quickPresets.emplace_back(1, 1);
        quickPresets.emplace_back(2, 1);
        quickPresets.emplace_back(3, 1);
        quickPresets.emplace_back(4, 1);
        return Project(std::move(instruments), std::move(parts), std::move(quickPresets));
    }

    Json::Value ToJson(const Instrument &instrument)
    {
        Json::Value result;
        result["lv2uri"] = instrument.Lv2Uri();
        result["shared"] = instrument.Shared();
        return result;
    }
    Instrument InstrumentFromJson(const Json::Value &v)
    {
        return Instrument(v["lv2uri"].asString(), v["shared"].asBool());
    }
    Json::Value ToJson(const Part &part)
    {
        Json::Value result;
        result["name"] = part.Name();
        result["jackmidiport"] = part.JackMidiPort();
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

        return Part(v["name"].asString(), v["jackmidiport"].asString(), v["midichannelforsharedinstruments"].asInt(), instrumentIndex, presetIndex, v["amplitudefactor"].asFloat());
    }
    Json::Value ToJson(const TQuickPreset &quickPreset)
    {
        Json::Value result;
        result["instrumentindex"] = quickPreset.InstrumentIndex();
        result["programindex"] = quickPreset.ProgramIndex();
        return result;
    }
    TQuickPreset QuickPresetFromJson(const Json::Value &v)
    {
        return TQuickPreset(v["instrumentindex"].asInt(), v["programindex"].asInt());
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
            result["quickpresets"].append(ToJson(quickPreset));
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
        std::vector<TQuickPreset> quickPresets;
        for (const auto &quickPreset : v["quickpresets"])
        {
            quickPresets.push_back(QuickPresetFromJson(quickPreset));
        }
        return Project(std::move(instruments), std::move(parts), std::move(quickPresets));
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
}