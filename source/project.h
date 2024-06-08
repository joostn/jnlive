#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <optional>
#include "json/json.h"

namespace project
{
    class TInstrument
    {
    public:
        TInstrument(std::string &&lv2Uri, bool shared, std::string &&name) : m_Lv2Uri(std::move(lv2Uri)), m_Shared(shared), m_Name(std::move(name))
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
    class TPart  // one for each keyboard
    {
    public:
        TPart(std::string &&name, int midiChannelForSharedInstruments, const std::optional<size_t>& activeInstrumentIndex, const std::optional<size_t>& activePresetIndex, float amplitudeFactor) : m_Name(std::move(name)), m_MidiChannelForSharedInstruments(midiChannelForSharedInstruments), m_ActiveInstrumentIndex(activeInstrumentIndex), m_ActivePresetIndex(activePresetIndex), m_AmplitudeFactor(amplitudeFactor)
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
        TPart ChangeActiveInstrumentIndex(std::optional<size_t> activeInstrumentIndex) const
        {
            auto result = *this;
            result.m_ActiveInstrumentIndex = activeInstrumentIndex;
            return result;
        }
        TPart ChangeActivePresetIndex(std::optional<size_t> activePresetIndex) const
        {
            auto result = *this;
            result.m_ActivePresetIndex = activePresetIndex;
            return result;
        }
        TPart ChangeAmplitudeFactor(float amplitudeFactor) const
        {
            auto result = *this;
            result.m_AmplitudeFactor = amplitudeFactor;
            return result;
        }
        std::vector<std::optional<size_t>> QuickPresets() const { return m_QuickPresets; }
        TPart ChangeQuickPreset(size_t quickpresetindex, std::optional<size_t> presetindex) const
        {
            auto result = *this;
            if(quickpresetindex >= result.m_QuickPresets.size())
            {
                result.m_QuickPresets.resize(quickpresetindex+1);
            }
            result.m_QuickPresets[quickpresetindex] = presetindex;
            return result;
        }

    private:
        std::string m_Name;
        int m_MidiChannelForSharedInstruments = 0;
        std::optional<size_t> m_ActiveInstrumentIndex;
        std::optional<size_t> m_ActivePresetIndex;
        float m_AmplitudeFactor = 1.0f;
        std::vector<std::optional<size_t>> m_QuickPresets;
    };
    class TPreset
    {
    public:
        TPreset(size_t instrumentIndex, std::string &&name, std::string &&presetSubDir) : m_InstrumentIndex(instrumentIndex), m_PresetSubDir(presetSubDir), m_Name(std::move(name)) {}
        size_t InstrumentIndex() const { return m_InstrumentIndex; }
        const std::string &Name() const {return m_Name;}
        const std::string &PresetSubDir() const {return m_PresetSubDir;}
        TPreset ChangeName(std::string &&name) const
        {
            auto result = *this;
            result.m_Name = name;
            return result;
        }
        TPreset ChangePresetSubDir(std::string &&presetSubDir) const
        {
            auto result = *this;
            result.m_PresetSubDir = presetSubDir;
            return result;
        }
        TPreset ChangeInstrumentIndex(size_t instrumentIndex) const
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
    
    class TProject
    {
    public:
        TProject() {}
        TProject(std::vector<TInstrument>&& instruments, std::vector<TPart>&& parts, std::vector<std::optional<TPreset>> &&presets, std::optional<size_t> focusedPart, bool showUi, TReverb &&reverb) : m_Instruments(std::move(instruments)), m_Parts(std::move(parts)), m_Presets(std::move(presets)), m_FocusedPart(focusedPart), m_ShowUi(showUi), m_Reverb(std::move(reverb))  {}
        const std::vector<TInstrument>& Instruments() const { return m_Instruments; }
        const std::vector<TPart>& Parts() const { return m_Parts; }
        const std::vector<std::optional<TPreset>>& Presets() const { return m_Presets; }
        TProject Change(std::optional<size_t> focusedPart, bool showUi) const
        {
            auto parts = m_Parts;
            auto instruments = m_Instruments;
            auto presets = m_Presets;
            auto reverb = Reverb();
            return TProject(std::move(instruments), std::move(parts), std::move(presets), focusedPart, showUi, std::move(reverb));
        }
        TProject ChangeReverb(TReverb &&reverb) const
        {
            auto parts = m_Parts;
            auto instruments = m_Instruments;
            auto presets = m_Presets;
            auto focusedPart = FocusedPart();
            auto showUi = ShowUi();
            return TProject(std::move(instruments), std::move(parts), std::move(presets), focusedPart, showUi, std::move(reverb));
        }
        TProject ChangePart(size_t partIndex, TPart &&part) const
        {
            std::vector<TPart> parts;
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
            auto presets = m_Presets;
            auto reverb = Reverb();
            return TProject(std::move(instruments), std::move(parts), std::move(presets), FocusedPart(), ShowUi(), std::move(reverb));
        }
        TProject SwitchToPreset(size_t partIndex, size_t presetIndex) const
        {
            if( (partIndex < Parts().size()) && (presetIndex < m_Presets.size()) )
            {
                const auto &preset = Presets()[presetIndex];
                if(preset)
                {
                    auto instrumentindex = preset.value().InstrumentIndex();
                    auto part = Parts().at(partIndex).ChangeActiveInstrumentIndex(instrumentindex).ChangeActivePresetIndex(presetIndex);
                    return ChangePart(partIndex, std::move(part));
                }
            }
            return *this;
        }
        TProject ChangePreset(size_t presetIndex, std::optional<TPreset> &&preset) const
        {
            auto presets = m_Presets;
            presets[presetIndex] = std::move(preset);
            auto parts = m_Parts;
            auto instruments = m_Instruments;
            auto reverb = Reverb();
            return TProject(std::move(instruments), std::move(parts), std::move(presets), FocusedPart(), ShowUi(), std::move(reverb));
        }
        TProject SwitchFocusedPartToPreset(size_t presetIndex) const
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
        void SetPresets(std::vector<std::optional<TPreset>> &&presets)
        {
            m_Presets = std::move(presets);
        }
        const TReverb& Reverb() const { return m_Reverb; }
        TProject AddInstrument(TInstrument &&inst) const
        {
            auto instruments = m_Instruments;
            instruments.push_back(std::move(inst));
            auto parts = m_Parts;
            auto presets = m_Presets;
            auto reverb = Reverb();
            return TProject(std::move(instruments), std::move(parts), std::move(presets), FocusedPart(), ShowUi(), std::move(reverb));
        }
        TProject DeleteInstrument(size_t index) const;

    private:
        std::vector<TPart> m_Parts;
        std::vector<TInstrument> m_Instruments;
        std::vector<std::optional<TPreset>> m_Presets;
        std::optional<size_t> m_FocusedPart;
        bool m_ShowUi = false;
        TReverb m_Reverb;
    };
    class TJackConnections
    {
    public:
        TJackConnections() {}
        TJackConnections(std::array<std::vector<std::string>, 2> &&audioOutputs, std::vector<std::vector<std::string>> &&midiInputs, std::vector<std::pair<std::string, std::vector<std::string>>> &&controllermidiports) : m_AudioOutputs(std::move(audioOutputs)), m_MidiInputs(std::move(midiInputs)), m_ControllerMidiPorts(std::move(controllermidiports)) {}
        const std::array<std::vector<std::string>, 2>& AudioOutputs() const { return m_AudioOutputs; }
        const std::vector<std::vector<std::string>>& MidiInputs() const { return m_MidiInputs; }
        const std::vector<std::pair<std::string /* id */, std::vector<std::string> /* jackname */>>& ControllerMidiPorts() const { return m_ControllerMidiPorts; }

    private:
        std::array<std::vector<std::string>, 2> m_AudioOutputs;
        std::vector<std::vector<std::string>> m_MidiInputs;
        std::vector<std::pair<std::string /* id */, std::vector<std::string> /* jacknames */>> m_ControllerMidiPorts; 
    };

    Json::Value ToJson(const TReverb &reverb);
    TReverb ReverbFromJson(const Json::Value &v);
    Json::Value ToJson(const TInstrument &instrument);
    TInstrument InstrumentFromJson(const Json::Value &v);
    Json::Value ToJson(const TPart &part);
    TPart PartFromJson(const Json::Value &v);
    Json::Value ToJson(const TPreset &preset);
    TPreset PresetFromJson(const Json::Value &v);
    Json::Value ToJson(const TProject &project);
    TProject ProjectFromJson(const Json::Value &v);
    TProject ProjectFromFile(const std::string &filename);
    void ProjectToFile(const TProject &project, const std::string &filename);
    TProject TestProject();

    Json::Value ToJson(const TJackConnections &jackConnections);
    TJackConnections JackConnectionsFromJson(const Json::Value &v);

    TJackConnections JackConnectionsFromFile(const std::string &filename);
    void JackConnectionsToFile(const TJackConnections &jackConnections, const std::string &filename);

}