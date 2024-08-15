#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <optional>
#include "json/json.h"

namespace project
{
    class THammondData
    {
    public:
        class TPart
        {
        public:
            TPart() : m_Registers({0,0,0,0,0,0,0,0,0})
            {
            }
            TPart(std::array<int, 9> &&registers) : m_Registers(std::move(registers))
            {
            }
            const std::array<int, 9>& Registers() const { return m_Registers; }
            TPart ChangeRegister(int index, int value) const
            {
                auto newregisters = m_Registers;
                newregisters[index] = value;
                return TPart(std::move(newregisters));
            }
            auto operator==(const TPart &other) const
            {
                return m_Registers == other.m_Registers;
            }
        private:
            std::array<int, 9> m_Registers;
        };
        THammondData() : m_Parts({TPart({8,8,8,0,0,0,0,0,0}), TPart({0,0,8,8,0,0,0,0,0})}), m_Percussion(true), m_PercussionSoft(false), m_PercussionFast(true), m_Percussion2ndHarmonic(true), m_OverDrive(false)
        {
        }
        THammondData ChangePart(int partindex, const TPart &part) const
        {
            auto newparts = m_Parts;
            newparts[partindex] = part;
            THammondData result = *this;
            result.m_Parts = std::move(newparts);
            return result;
        }
        const TPart& Part(int partindex) const
        {
            return m_Parts.at(partindex);
        }
        bool Percussion() const { return m_Percussion; }
        bool PercussionSoft() const { return m_PercussionSoft; }
        bool PercussionFast() const { return m_PercussionFast; }
        bool Percussion2ndHarmonic() const { return m_Percussion2ndHarmonic; }
        bool OverDrive() const { return m_OverDrive; }
        THammondData ChangePercussion(bool percussion) const
        {
            auto result = *this;
            result.m_Percussion = percussion;
            return result;
        }
        THammondData ChangePercussionSoft(bool PercussionSoft) const
        {
            auto result = *this;
            result.m_PercussionSoft = PercussionSoft;
            return result;
        }
        THammondData ChangePercussionFast(bool PercussionFast) const
        {
            auto result = *this;
            result.m_PercussionFast = PercussionFast;
            return result;
        }
        THammondData ChangePercussion2ndHarmonic(bool percussion2ndHarmonic) const
        {
            auto result = *this;
            result.m_Percussion2ndHarmonic = percussion2ndHarmonic;
            return result;
        }        
        THammondData ChangeOverDrive(bool Overdrive) const
        {
            auto result = *this;
            result.m_OverDrive = Overdrive;
            return result;
        }        
        inline auto Tuple() const
        {
            return std::tie(m_Parts, m_Percussion, m_PercussionSoft, m_PercussionFast, m_Percussion2ndHarmonic, m_OverDrive);
        }
        auto operator==(const THammondData &other) const
        {
            return Tuple() == other.Tuple();
        }

    private:
        std::array<TPart, 2> m_Parts;
        bool m_Percussion = false;
        bool m_PercussionSoft = false;
        bool m_PercussionFast = false;
        bool m_Percussion2ndHarmonic = false;
        bool m_OverDrive = false;
    };
        
    class TInstrument
    {
    public:
        class TParameter
        {
        public:
            TParameter(int controllerNumber, std::optional<int> initialValue, std::string &&label) : m_ControllerNumber(controllerNumber), m_InitialValue(initialValue), m_Label(std::move(label))
            {
            }
            int ControllerNumber() const
            {
                return m_ControllerNumber;
            }
            std::optional<int> InitialValue() const
            {
                return m_InitialValue;
            }
            const std::string& Label() const
            {
                return m_Label;
            }
            auto Tuple() const
            {
                return std::tie(m_ControllerNumber, m_InitialValue, m_Label);
            }
            bool operator==(const TParameter &other) const
            {
                return Tuple() == other.Tuple();
            }
            TParameter ChangeInitialValue(std::optional<int> initialValue) const
            {
                auto result = *this;
                result.m_InitialValue = initialValue;
                return result;
            }
            TParameter ChangeLabel(std::string &&label) const
            {
                auto result = *this;
                result.m_Label = label;
                return result;
            }
            TParameter ChangeControllerNumber(int controllerNumber) const
            {
                auto result = *this;
                result.m_ControllerNumber = controllerNumber;
                return result;
            }
        private:
            int m_ControllerNumber;
            std::optional<int> m_InitialValue;
            std::string m_Label;
        };
    public:
        TInstrument(std::string &&lv2Uri, bool ishammond, std::string &&name, std::vector<TParameter> &&parameters, bool hasVocoderInput) : m_Lv2Uri(std::move(lv2Uri)), m_IsHammond(ishammond), m_Name(std::move(name)), m_HasVocoderInput(hasVocoderInput), m_Parameters(std::move(parameters))
        {
        }
        const std::string& Lv2Uri() const
        {
            return m_Lv2Uri;
        }
        bool IsHammond() const
        {
            return m_IsHammond;
        }
        const std::string& Name() const
        {
            return m_Name;
        }
        const std::vector<TParameter>& Parameters() const
        {
            return m_Parameters;
        }
        auto Tuple() const
        {
            return std::tie(m_Lv2Uri, m_Name, m_IsHammond, m_Parameters, m_HasVocoderInput);
        }
        bool HasVocoderInput() const
        {
            return m_HasVocoderInput;
        }
        bool operator==(const TInstrument &other) const
        {
            return Tuple() == other.Tuple();
        }
        TInstrument ChangeLv2Uri(std::string &&lv2Uri) const
        {
            auto result = *this;
            result.m_Lv2Uri = std::move(lv2Uri);
            return result;
        }
        TInstrument ChangeName(std::string &&name) const
        {
            auto result = *this;
            result.m_Name = std::move(name);
            return result;
        }
        TInstrument ChangeIsHammond(bool ishammond) const
        {
            auto result = *this;
            result.m_IsHammond = ishammond;
            return result;
        }
        TInstrument ChangeParameters(std::vector<TParameter> &&parameters) const
        {
            auto result = *this;
            result.m_Parameters = std::move(parameters);
            return result;
        }
        TInstrument ChangeHasVocoderInput(bool hasVocoderInput) const
        {
            auto result = *this;
            result.m_HasVocoderInput = hasVocoderInput;
            return result;
        }

    private:
        std::string m_Lv2Uri;
        std::string m_Name;
        bool m_IsHammond = false;  // for hammond organ, etc: 2 keyboards per instrument
        std::vector<TParameter> m_Parameters;
        bool m_HasVocoderInput = false;
    };
    class TPart  // one for each keyboard
    {
    public:
        TPart(std::string &&name) : m_Name(std::move(name)) {}
        TPart(std::string &&name, int midiChannelForSharedInstruments, const std::optional<size_t>& activeInstrumentIndex, const std::optional<size_t>& activePresetIndex, std::vector<std::optional<size_t>> &&quickPresets, float amplitudeFactor) : m_Name(std::move(name)), m_MidiChannelForSharedInstruments(midiChannelForSharedInstruments), m_ActiveInstrumentIndex(activeInstrumentIndex), m_ActivePresetIndex(activePresetIndex), m_AmplitudeFactor(amplitudeFactor), m_QuickPresets(std::move(quickPresets))
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
        TPart ChangeName(std::string &&name) const
        {
            auto result = *this;
            result.m_Name = name;
            return result;
        }
        TPart ChangeMidiChannelForSharedInstruments(int midiChannelForSharedInstruments) const
        {
            auto result = *this;
            result.m_MidiChannelForSharedInstruments = midiChannelForSharedInstruments;
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
        auto Tuple() const
        {
            return std::tie(m_Name, m_MidiChannelForSharedInstruments, m_ActiveInstrumentIndex, m_ActivePresetIndex, m_AmplitudeFactor, m_QuickPresets);
        }
        bool operator==(const TPart &other) const
        {
            return Tuple() == other.Tuple();
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
        TPreset(size_t instrumentIndex, std::string &&name, std::string &&presetSubDir, std::optional<std::vector<TInstrument::TParameter>> &&overrideParameters) : m_InstrumentIndex(instrumentIndex), m_PresetSubDir(presetSubDir), m_Name(std::move(name)), m_OverrideParameters(std::move(overrideParameters)) {}
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
        TPreset ChangeOverrideParameters(std::optional<std::vector<TInstrument::TParameter>> &&overrideParameters) const
        {
            auto result = *this;
            result.m_OverrideParameters = std::move(overrideParameters);
            return result;
        }
        const std::optional<std::vector<TInstrument::TParameter>>& OverrideParameters() const {return m_OverrideParameters;}
        auto Tuple() const
        {
            return std::tie(m_InstrumentIndex, m_Name, m_PresetSubDir, m_OverrideParameters);
        }
        bool operator==(const TPreset &other) const
        {
            return Tuple() == other.Tuple();
        }

    private:
        size_t m_InstrumentIndex = 0;
        std::string m_Name;
        std::string m_PresetSubDir;
        std::optional<std::vector<TInstrument::TParameter>> m_OverrideParameters;
    };
    class TReverb
    {
    public:
        TReverb() {}
        TReverb(std::string &&reverbPresetSubDir, std::string &&reverbLv2Uri, float mixLevel) : m_ReverbPresetSubDir(reverbPresetSubDir), m_ReverbLv2Uri(reverbLv2Uri), m_MixLevel(mixLevel) {}
        const std::string &ReverbPresetSubDir() const {return m_ReverbPresetSubDir;}
        const std::string &ReverbLv2Uri() const {return m_ReverbLv2Uri;}
        float MixLevel() const {return m_MixLevel;}
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
        auto Tuple() const
        {
            return std::tie(m_ReverbPresetSubDir, m_ReverbLv2Uri, m_MixLevel);
        }
        bool operator==(const TReverb &other) const
        {
            return Tuple() == other.Tuple();
        }

    private:
        std::string m_ReverbPresetSubDir; // or empty for default preset
        std::string m_ReverbLv2Uri;       // or empty (no reverb)
        float m_MixLevel = 0.2f;
    };
    
    class TProject
    {
    public:
        TProject() {}
        TProject(std::vector<TInstrument>&& instruments, std::vector<TPart>&& parts, std::vector<std::optional<TPreset>> &&presets, TReverb &&reverb) : m_Instruments(std::move(instruments)), m_Parts(std::move(parts)), m_Presets(std::move(presets)), m_Reverb(std::move(reverb))  {}
        const std::vector<TInstrument>& Instruments() const { return m_Instruments; }
        const std::vector<TPart>& Parts() const { return m_Parts; }
        const std::vector<std::optional<TPreset>>& Presets() const { return m_Presets; }
        TProject ChangeReverb(TReverb &&reverb) const
        {
            auto result = *this;
            result.m_Reverb = std::move(reverb);
            return result;
        }
        TProject ChangePart(size_t partIndex, TPart &&part) const
        {
            auto result = *this;
            if(part.ActivePresetIndex() && (*part.ActivePresetIndex() >= m_Presets.size()))
            {
                part = part.ChangeActivePresetIndex(std::nullopt);
            }
            if(partIndex < m_Parts.size())
            {
                const auto &oldpart = m_Parts[partIndex];
                if(part.ActivePresetIndex() && (oldpart.ActivePresetIndex() != part.ActivePresetIndex()))
                {
                    // switch to new preset
                    auto presetIndex = part.ActivePresetIndex().value();
                    const auto &preset = Presets().at(presetIndex);
                    if(preset)
                    {
                        auto instrumentindex = preset.value().InstrumentIndex();
                        part = part.ChangeActiveInstrumentIndex(instrumentindex);
                    }
                }
                result.m_Parts[partIndex] = std::move(part);
            }
            return result;
        }
        TProject AddPart(TPart &&part) const
        {
            auto result = *this;
            result.m_Parts.emplace_back(std::move(part));
            return result;
        }
        TProject DeletePart(size_t partindex) const
        {
            auto result = *this;
            if(partindex < m_Parts.size())
            {
                result.m_Parts.erase(result.m_Parts.begin() + partindex);
            }
            return result;
        }
        TProject SwitchToPreset(size_t partIndex, size_t presetIndex) const
        {
            return ChangePart(partIndex, m_Parts[partIndex].ChangeActivePresetIndex(presetIndex));
        }
        TProject ChangePreset(size_t presetIndex, std::optional<TPreset> &&preset) const
        {
            auto result = *this;
            if( (presetIndex >= m_Presets.size()) && (preset))
            {
                result.m_Presets.resize(presetIndex+1);
            }
            if( (presetIndex < m_Presets.size()) && (preset))
            {
                result.m_Presets[presetIndex] = std::move(preset);
            }
            return result;
        }
        void SetPresets(std::vector<std::optional<TPreset>> &&presets)
        {
            m_Presets = std::move(presets);
        }
        const TReverb& Reverb() const { return m_Reverb; }
        TProject AddInstrument(TInstrument &&inst) const
        {
            auto result = *this;
            result.m_Instruments.push_back(std::move(inst));
            return result;
        }
        TProject ChangeInstrument(size_t index, TInstrument &&inst) const
        {
            auto result = *this;
            result.m_Instruments.at(index) = std::move(inst);
            return result;            
        }
        TProject DeleteInstrument(size_t index) const;
        auto Tuple() const
        {
            return std::tie(m_Instruments, m_Parts, m_Presets, m_Reverb);
        }
        bool operator==(const TProject &other) const
        {
            return Tuple() == other.Tuple();
        }
        std::vector<TInstrument::TParameter> ParametersForPart(size_t partindex) const
        {
            std::vector<TInstrument::TParameter> result;
            const auto &part = Parts().at(partindex);
            if(part.ActiveInstrumentIndex())
            {
                if(part.ActivePresetIndex() && Presets().at(*part.ActivePresetIndex()) && Presets().at(*part.ActivePresetIndex()).value().OverrideParameters())
                {
                    result = Presets().at(*part.ActivePresetIndex()).value().OverrideParameters().value();
                }
                else
                {
                    result = Instruments().at(part.ActiveInstrumentIndex().value()).Parameters();
                }
            }
            return result;
        }

    private:
        std::vector<TPart> m_Parts;
        std::vector<TInstrument> m_Instruments;
        std::vector<std::optional<TPreset>> m_Presets;
        TReverb m_Reverb;
    };
    class TJackConnections
    {
    public:
        TJackConnections() {}
        TJackConnections(std::array<std::vector<std::string>, 2> &&audioOutputs, std::vector<std::vector<std::string>> &&midiInputs, std::vector<std::pair<std::string, std::vector<std::string>>> &&controllermidiports, std::vector<std::string> &&vocoderInput) : m_AudioOutputs(std::move(audioOutputs)), m_MidiInputs(std::move(midiInputs)), m_ControllerMidiPorts(std::move(controllermidiports)), m_VocoderInput(std::move(vocoderInput)) {}
        const std::array<std::vector<std::string>, 2>& AudioOutputs() const { return m_AudioOutputs; }
        const std::vector<std::vector<std::string>>& MidiInputs() const { return m_MidiInputs; }
        const std::vector<std::pair<std::string /* id */, std::vector<std::string> /* jackname */>>& ControllerMidiPorts() const { return m_ControllerMidiPorts; }
        const std::vector<std::string>& VocoderInput() const { return m_VocoderInput; }

    private:
        std::array<std::vector<std::string>, 2> m_AudioOutputs;
        std::vector<std::vector<std::string>> m_MidiInputs;
        std::vector<std::pair<std::string /* id */, std::vector<std::string> /* jacknames */>> m_ControllerMidiPorts; 
        std::vector<std::string> m_VocoderInput;
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