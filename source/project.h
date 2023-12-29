#pragma once
#include <vector>
#include <string>
#include <fstream>
#include "json/json.h"

namespace project
{
    class Instrument
    {
    public:
        Instrument(std::string &&lv2Uri, bool shared) : m_Lv2Uri(std::move(lv2Uri)), m_Shared(shared)
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

    private:
        std::string m_Lv2Uri;
        bool m_Shared = false;  // for hammond organ, etc: 2 keyboards per instrument
    };
    class Part  // one for each keyboard
    {
    public:
        Part(std::string &&name, std::string &&jackMidiPort, int midiChannelForSharedInstruments) : m_Name(std::move(name)), m_JackMidiPort(std::move(jackMidiPort)), m_MidiChannelForSharedInstruments(midiChannelForSharedInstruments)
        {
        }
        const std::string& Name() const
        {
            return m_Name;
        }
        const std::string& JackMidiPort() const
        {
            return m_JackMidiPort;
        }
        const int& MidiChannelForSharedInstruments() const
        {
            return m_MidiChannelForSharedInstruments;
        }
    private:
        std::string m_Name;
        std::string m_JackMidiPort;
        int m_MidiChannelForSharedInstruments = 0;
    };
    class TQuickPreset
    {
    public:
        TQuickPreset(size_t instrumentIndex, size_t programIndex) : m_InstrumentIndex(instrumentIndex), m_ProgramIndex(programIndex) {}
        size_t InstrumentIndex() const { return m_InstrumentIndex; }
        size_t ProgramIndex() const { return m_ProgramIndex; }
    private:
        size_t m_InstrumentIndex = 0;
        size_t m_ProgramIndex = 0;
    };
    class Project
    {
    public:
        Project() {}
        Project(std::vector<Instrument>&& instruments, std::vector<Part>&& parts, std::vector<TQuickPreset> &&quickPresets) : m_Instruments(std::move(instruments)), m_Parts(std::move(parts)), m_QuickPresets(std::move(quickPresets))  {}
        const std::vector<Instrument>& Instruments() const { return m_Instruments; }
        const std::vector<Part>& Parts() const { return m_Parts; }
        const std::vector<TQuickPreset>& QuickPresets() const { return m_QuickPresets; }
    private:
        std::vector<Part> m_Parts;
        std::vector<Instrument> m_Instruments;
        std::vector<TQuickPreset> m_QuickPresets;
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

}