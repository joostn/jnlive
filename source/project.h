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
        Part ChangeActiveInstrumentIndex(std::optional<size_t> activeInstrumentIndex) const
        {
            auto result = *this;
            result.m_ActiveInstrumentIndex = activeInstrumentIndex;
            return result;
        }
        Part ChangeActivePresetIndex(std::optional<size_t> activePresetIndex) const
        {
            auto result = *this;
            result.m_ActivePresetIndex = activePresetIndex;
            return result;
        }
        Part ChangeAmplitudeFactor(float amplitudeFactor) const
        {
            auto result = *this;
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
        TQuickPreset ChangeName(std::string &&name) const
        {
            auto result = *this;
            result.m_Name = name;
            return result;
        }
        TQuickPreset ChangePresetSubDir(std::string &&presetSubDir) const
        {
            auto result = *this;
            result.m_PresetSubDir = presetSubDir;
            return result;
        }
        TQuickPreset ChangeInstrumentIndex(size_t instrumentIndex) const
        {
            auto result = *this;
            result.m_InstrumentIndex = instrumentIndex;
            return result;
        }
    private:
        size_t m_InstrumentIndex = 0;
        std::string m_Name;
        std::string m_PresetSubDir;
    };
    class TReverb
    {
    public:
        TReverb() {}
        TReverb(std::string &&reverbPresetSubDir, std::string &&reverbLv2Uri, float mixLevel, bool showGui) : m_ReverbPresetSubDir(reverbPresetSubDir), m_ReverbLv2Uri(reverbLv2Uri), m_MixLevel(mixLevel), m_ShowGui(showGui) {}
        const std::string &ReverbPresetSubDir() const {return m_ReverbPresetSubDir;}
        const std::string &ReverbLv2Uri() const {return m_ReverbLv2Uri;}
        float MixLevel() const {return m_MixLevel;}
        bool ShowGui() const {return m_ShowGui;}
        TReverb ChangeShowGui(bool showgui) const
        {
            auto result = *this;
            result.m_ShowGui = showgui;
            return result;
        }
        TReverb ChangeMixLevel(float mixLevel) const
        {
            auto result = *this;
            result.m_MixLevel = mixLevel;
            return result;
        }
        TReverb ChangeReverbPresetSubDir(std::string &&reverbPresetSubDir) const
        {
            auto result = *this;
            result.m_ReverbPresetSubDir = reverbPresetSubDir;
            return result;
        }
        TReverb ChangeReverbLv2Uri(std::string &&reverbPresetSubDir) const
        {
            auto result = *this;
            result.m_ReverbLv2Uri = reverbPresetSubDir;
            return result;
        }
    private:
        std::string m_ReverbPresetSubDir; // or empty for default preset
        std::string m_ReverbLv2Uri;       // or empty (no reverb)
        float m_MixLevel = 0.2f;
        bool m_ShowGui = false;
    };
    
    class Project
    {
    public:
        Project() {}
        Project(std::vector<Instrument>&& instruments, std::vector<Part>&& parts, std::vector<std::optional<TQuickPreset>> &&quickPresets, std::optional<size_t> focusedPart, bool showUi, TReverb &&reverb) : m_Instruments(std::move(instruments)), m_Parts(std::move(parts)), m_QuickPresets(std::move(quickPresets)), m_FocusedPart(focusedPart), m_ShowUi(showUi), m_Reverb(std::move(reverb))  {}
        const std::vector<Instrument>& Instruments() const { return m_Instruments; }
        const std::vector<Part>& Parts() const { return m_Parts; }
        const std::vector<std::optional<TQuickPreset>>& QuickPresets() const { return m_QuickPresets; }
        Project Change(std::optional<size_t> focusedPart, bool showUi) const
        {
            auto parts = m_Parts;
            auto instruments = m_Instruments;
            auto quickPresets = m_QuickPresets;
            auto reverb = Reverb();
            return Project(std::move(instruments), std::move(parts), std::move(quickPresets), focusedPart, showUi, std::move(reverb));
        }
        Project ChangeReverb(TReverb &&reverb) const
        {
            auto parts = m_Parts;
            auto instruments = m_Instruments;
            auto quickPresets = m_QuickPresets;
            auto focusedPart = FocusedPart();
            auto showUi = ShowUi();
            return Project(std::move(instruments), std::move(parts), std::move(quickPresets), focusedPart, showUi, std::move(reverb));
        }
        Project ChangePart(size_t partIndex, Part &&part) const
        {
            std::vector<Part> parts;
            for(size_t i=0; i < m_Parts.size(); ++i)
            {
                if(i == partIndex)
                {
                    parts.push_back(std::move(part));
                }
                else
                {
                    parts.push_back(m_Parts[i]);
                }
            }
            auto instruments = m_Instruments;
            auto quickPresets = m_QuickPresets;
            auto reverb = Reverb();
            return Project(std::move(instruments), std::move(parts), std::move(quickPresets), FocusedPart(), ShowUi(), std::move(reverb));
        }
        Project SwitchToPreset(size_t partIndex, size_t presetIndex) const
        {
            if( (partIndex < Parts().size()) && (presetIndex < m_QuickPresets.size()) )
            {
                const auto &preset = QuickPresets()[presetIndex];
                if(preset)
                {
                    auto instrumentindex = preset.value().InstrumentIndex();
                    auto part = Parts().at(partIndex).ChangeActiveInstrumentIndex(instrumentindex).ChangeActivePresetIndex(presetIndex);
                    return ChangePart(partIndex, std::move(part));
                }
            }
            return *this;
        }
        Project ChangePreset(size_t presetIndex, std::optional<TQuickPreset> &&preset) const
        {
            auto quickPresets = m_QuickPresets;
            quickPresets[presetIndex] = std::move(preset);
            auto parts = m_Parts;
            auto instruments = m_Instruments;
            auto reverb = Reverb();
            return Project(std::move(instruments), std::move(parts), std::move(quickPresets), FocusedPart(), ShowUi(), std::move(reverb));
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
        const TReverb& Reverb() const { return m_Reverb; }
        Project AddInstrument(Instrument &&inst) const
        {
            auto instruments = m_Instruments;
            instruments.push_back(std::move(inst));
            auto parts = m_Parts;
            auto quickPresets = m_QuickPresets;
            auto reverb = Reverb();
            return Project(std::move(instruments), std::move(parts), std::move(quickPresets), FocusedPart(), ShowUi(), std::move(reverb));
        }
        Project DeleteInstrument(size_t index) const;

    private:
        std::vector<Part> m_Parts;
        std::vector<Instrument> m_Instruments;
        std::vector<std::optional<TQuickPreset>> m_QuickPresets;
        std::optional<size_t> m_FocusedPart;
        bool m_ShowUi = false;
        TReverb m_Reverb;
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

    Json::Value ToJson(const TReverb &reverb);
    TReverb ReverbFromJson(const Json::Value &v);
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