#pragma once
#include <array>
#include <algorithm>
#include <stdexcept>
#include <span>
#include <iostream>
#include <vector>
#include <optional>

namespace midi
{
    static constexpr int ccSustainPedal = 64;
    static constexpr int ccModWheel = 1;
    static constexpr int ccVolume = 7;
    static constexpr int ccPan = 10;
    static constexpr int ccExpression = 11;
    class SimpleEvent
    {
    public:
        enum class Type {
            NoteOff = 0x80,
            NoteOn = 0x90,
            PolyphonicKeyPressure = 0xA0,
            ControlChange = 0xB0,
            ProgramChange = 0xC0,
            ChannelPressure = 0xD0,
            PitchBendChange = 0xE0,
            System = 0xF0,
        };
        static bool IsSupported(const void *buf, size_t size);
        SimpleEvent(const void *buf, size_t size);
        int Channel() const { return m_Data[0] & 0x0F; }
        Type type() const { return (Type)(m_Data[0] & 0xF0); }
        const char* Buffer() const { return m_Data.data(); }
        size_t Size() const 
        {
            return ExpectedSize(m_Data[0]);
        }
        std::span<const char> Span() const
        {
            return {m_Data.data(), Size()};
        }
        static size_t ExpectedSize(char byte0);
        std::optional<SimpleEvent> Transpose(int delta) const;
        static SimpleEvent NoteOn(int channel, int note, int velocity);
        static SimpleEvent NoteOff(int channel, int note, int velocity);
        static SimpleEvent ControlChange(int channel, int cc, int value);
        static SimpleEvent ProgramChange(int channel, int p);
        SimpleEvent ChangeChannel(int channel) const;
        static SimpleEvent AllNotesOff(int channel);
        auto operator<=>(const SimpleEvent&) const = default;
        void ToDebugStream(std::ostream &str) const;
        std::string ToDebugString() const;
        int NoteNumber() const
        {
            return m_Data[1];
        }
        int Velocity() const
        {
            return m_Data[2];
        }
        int ControlNumber() const
        {
            return m_Data[1];
        }
        int ControlValue() const
        {
            return m_Data[2];
        }

    private:
        SimpleEvent(const std::array<char, 3> &data) : m_Data(data) {}

    private:
        std::array<char, 3> m_Data {0,0,0};
    };
    class TMidiOrSysexEvent
    {
    public:
        static bool IsSupported(const void *buf, size_t size)
        {
            if(SimpleEvent::IsSupported(buf, size))
            {
                return true;
            }
            if(size < 3)
            {
                return false;
            }
            auto charbuf = (const char*)buf;
            if( (charbuf[0] == (char)0xf0) && (charbuf[size-1] == (char)0xf7) )
            {
                // a complete sysex
                return true;
            }
            return false; // illegal message
        }
        TMidiOrSysexEvent(const SimpleEvent &event) : m_Event(event)
        {
        }
        TMidiOrSysexEvent(const void *buf, size_t size)
        {
            if(!IsSupported(buf, size))
            {
                throw std::runtime_error("midi event not supported");
            }
            if(SimpleEvent::IsSupported(buf, size))
            {
                m_Event = SimpleEvent(buf, size);
            }
            else
            {
                m_SysexEvent.resize(size);
                std::copy((const char*)buf, (const char*)buf + size, m_SysexEvent.begin());
            }
        }
        bool IsSysex() const
        {
            return !m_Event;
        }
        const SimpleEvent& GetSimpleEvent() const
        {
            if(!m_Event)
            {
                throw std::runtime_error("It's a sysex event");
            }
            return *m_Event;
        }
        std::span<const char> GetSysexEvent() const
        {
            return m_SysexEvent;
        }
        std::span<const char> Span() const
        {
            if(IsSysex())
            {
                return GetSysexEvent();
            }
            else
            {
                return GetSimpleEvent().Span();
            }
        }
        void ToDebugStream(std::ostream &str) const;
        std::string ToDebugString() const;
        TMidiOrSysexEvent ChangeChannel(int channel) const
        {
            if(IsSysex())
            {
                return *this;
            }
            return TMidiOrSysexEvent(GetSimpleEvent().ChangeChannel(channel));
        }

    private:
        std::optional<SimpleEvent> m_Event;
        std::vector<char> m_SysexEvent;
    };
}