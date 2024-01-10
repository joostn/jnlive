#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <optional>
#include "json/json.h"

namespace project
{
    class Instrument
    {
    public:
        Instrument(std::string &&lv2Uri, bool shared, std::string &&name) : m_Lv2Uri(std::move(lv2Uri)), m_Shared(shared), m_Name(std::move(name))
        {
        }
        const std::string& Lv2Uri() const
        {
            return m_Lv2Uri;
        }
        bool Shared() const
        {
            return m_Shared;
        }
        const std::string& Name() const
        {
            return m_Name;
        }

    private:
        std::string m_Lv2Uri;
        std::string m_Name;
        bool m_Shared = false;  // for hammond organ, etc: 2 keyboards per instrument
    };
    class Part  // one for each keyboard
    {
    public:
        Part(std::string &&name, int midiChannelForSharedInstruments, const std::optional<size_t>& activeInstrumentIndex, const std::optional<size_t>& activePresetIndex, float amplitudeFactor) : m_Name(std::move(name)), m_MidiChannelForSharedInstruments(midiChannelForSharedInstruments), m_ActiveInstrumentIndex(activeInstrumentIndex), m_ActivePresetIndex(activePresetIndex), m_AmplitudeFactor(amplitudeFactor)
        {
        }
        const std::string& Name() const
        {
            return m_Name;
        }
        const int& MidiChannelForSharedInstruments() const
        {
            return m_MidiChannelForSharedInstruments;
        }
        const std::optional<size_t>& ActiveInstrumentIndex() const { return m_ActiveInstrumentIndex; }
        const std::optional<size_t>& ActivePresetIndex() const { return m_ActivePresetIndex; }
        float AmplitudeFactor() const { return m_AmplitudeFactor; }
        Part Change(std::optional<size_t> activeInstrumentIndex,
        std::optional<size_t> activePresetIndex,
        float amplitudeFactor) const
        {
            auto result = *this;
            result.m_ActiveInstrumentIndex = activeInstrumentIndex;
            result.m_ActivePresetIndex = activePresetIndex;
            result.m_AmplitudeFactor = amplitudeFactor;
            return result;
        }

    private:
        std::string m_Name;
        int m_MidiChannelForSharedInstruments = 0;
        std::optional<size_t> m_ActiveInstrumentIndex;
        std::optional<size_t> m_ActivePresetIndex;
        float m_AmplitudeFactor = 1.0f;
    };
    class TQuickPreset
    {
    public:
        TQuickPreset(size_t instrumentIndex, std::string &&name, std::string &&presetSubDir) : m_InstrumentIndex(instrumentIndex), m_PresetSubDir(presetSubDir), m_Name(std::move(name)) {}
        size_t InstrumentIndex() const { return m_InstrumentIndex; }
        const std::string &Name() const {return m_Name;}
        const std::string &PresetSubDir() const {return m_PresetSubDir;}
    private:
        size_t m_InstrumentIndex = 0;
        std::string m_Name;
        std::string m_PresetSubDir;
    };
    class Project
    {
    public:
        Project() {}
        Project(std::vector<Instrument>&& instruments, std::vector<Part>&& parts, std::vector<std::optional<TQuickPreset>> &&quickPresets, std::optional<size_t> focusedPart, bool showUi) : m_Instruments(std::move(instruments)), m_Parts(std::move(parts)), m_QuickPresets(std::move(quickPresets)), m_FocusedPart(focusedPart), m_ShowUi(showUi)  {}
        const std::vector<Instrument>& Instruments() const { return m_Instruments; }
        const std::vector<Part>& Parts() const { return m_Parts; }
        const std::vector<std::optional<TQuickPreset>>& QuickPresets() const { return m_QuickPresets; }
        Project Change(std::optional<size_t> focusedPart, bool showUi) const
        {
            auto parts = m_Parts;
            auto instruments = m_Instruments;
            auto quickPresets = m_QuickPresets;
            return Project(std::move(instruments), std::move(parts), std::move(quickPresets), focusedPart, showUi);
        }
        Project ChangePart(size_t partIndex, std::optional<size_t> activeInstrumentIndex,
        std::optional<size_t> activePresetIndex,
        float amplitudeFactor) const
        {
            std::vector<Part> parts;
            for(size_t i=0; i < m_Parts.size(); ++i)
            {
                if(i == partIndex)
                {
                    parts.push_back(m_Parts[i].Change(activeInstrumentIndex, activePresetIndex, amplitudeFactor));
                }
                else
                {
                    parts.push_back(m_Parts[i]);
                }
            }
            auto instruments = m_Instruments;
            auto quickPresets = m_QuickPresets;
            return Project(std::move(instruments), std::move(parts), std::move(quickPresets), FocusedPart(), ShowUi());
        }
        Project SwitchToPreset(size_t partIndex, size_t presetIndex) const
        {
            if( (partIndex < Parts().size()) && (presetIndex < m_QuickPresets.size()) )
            {
                const auto &preset = QuickPresets()[presetIndex];
                if(preset)
                {
                    auto instrumentindex = preset.value().InstrumentIndex();
                    return ChangePart(partIndex, instrumentindex, presetIndex, Parts()[partIndex].AmplitudeFactor());
                }
            }
            return *this;
        }
        Project SwitchFocusedPartToPreset(size_t presetIndex) const
        {
            if(FocusedPart())
            {
                return SwitchToPreset(*FocusedPart(), presetIndex);
            }
            else
            {
                return *this;
            }
        }
        const bool& ShowUi() const { return m_ShowUi; }
        const std::optional<size_t>& FocusedPart() const { return m_FocusedPart; }
        void SetPresets(std::vector<std::optional<TQuickPreset>> &&quickPresets)
        {
            m_QuickPresets = std::move(quickPresets);
        }

    private:
        std::vector<Part> m_Parts;
        std::vector<Instrument> m_Instruments;
        std::vector<std::optional<TQuickPreset>> m_QuickPresets;
        std::optional<size_t> m_FocusedPart;
        bool m_ShowUi = false;
    };
    class TJackConnections
    {
    public:
        TJackConnections() {}
        TJackConnections(std::array<std::string, 2> &&audioOutputs, std::vector<std::string> &&midiInputs) : m_AudioOutputs(std::move(audioOutputs)), m_MidiInputs(std::move(midiInputs)) {}
        const std::array<std::string, 2>& AudioOutputs() const { return m_AudioOutputs; }
        const std::vector<std::string>& MidiInputs() const { return m_MidiInputs; }

    private:
        std::array<std::string, 2> m_AudioOutputs;
        std::vector<std::string> m_MidiInputs;
    };

    Json::Value ToJson(const Instrument &instrument);
    Instrument InstrumentFromJson(const Json::Value &v);
    Json::Value ToJson(const Part &part);
    Part PartFromJson(const Json::Value &v);
    Json::Value ToJson(const TQuickPreset &quickPreset);
    TQuickPreset QuickPresetFromJson(const Json::Value &v);
    Json::Value ToJson(const Project &project);
    Project ProjectFromJson(const Json::Value &v);
    Project ProjectFromFile(const std::string &filename);
    void ProjectToFile(const Project &project, const std::string &filename);
    Project TestProject();

    Json::Value ToJson(const TJackConnections &jackConnections);
    TJackConnections JackConnectionsFromJson(const Json::Value &v);

    TJackConnections JackConnectionsFromFile(const std::string &filename);
    void JackConnectionsToFile(const TJackConnections &jackConnections, const std::string &filename);

}