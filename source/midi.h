#pragma once
#include <array>
#include <algorithm>
#include <stdexcept>
#include <span>

namespace midi
{
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
        static bool IsSupported(const void *buf, size_t size)
        {
            if(size < 1)
            {
                return false;
            }
            auto buf0 = ((const char*)buf)[0];
            if(buf0 == (char)0xf0)
            {
                // start of exclusive
                return false;
            }
            if( (buf0 & 0x80) == 0)
            {
                // illegal message, we should start with a high bit set
                return false;
            }
            auto expectedsize = ExpectedSize(buf0);
            if(size != expectedsize)
            {
                return false;
            }
            return true;
        }
        SimpleEvent(const void *buf, size_t size)
        {
            if(!IsSupported(buf, size))
            {
                throw std::runtime_error("midi event not supported");
            }
            std::copy((const char*)buf, (const char*)buf + size, m_Data.begin());
        }
        int Channel() const { return m_Data[0] & 0x0F; }
        Type type() const { return (Type)(m_Data[0] & 0xF0); }
        const char* Buffer() const { return m_Data.data(); }
        size_t Size() const 
        {
            return ExpectedSize(m_Data[0]);
        }
        static size_t ExpectedSize(char byte0)
        { 
            if( (byte0 & 0x80) == 0)
            {
                // illegal
                return 0;
            }
            auto type = (Type)(byte0 & 0xF0);
            if(type == Type::ProgramChange || type == Type::ChannelPressure)
            {
                return 2;
            }
            else if(type == Type::System)
            {
                int subtype = (int)(byte0 & 0x0F);
                if(subtype == 0)
                {
                    // start of exclusive, can be any size, unsupported:
                    return 0;
                }
                else if( (subtype == 1) || (subtype == 3) )
                {
                    // time code, song select:
                    return 2;
                }
                else if(subtype == 2)
                {
                    // Song Position Pointer
                    return 3;
                }
                else
                {
                    // all other system messages are single byte:
                    return 1;
                }
            }
            else
            {
                // all other regular messages:
                return 3;
            }
        }
        static SimpleEvent NoteOn(int channel, int note, int velocity)
        {
            if(velocity < 1) velocity = 1;
            if(velocity > 127) velocity = 127;
            return SimpleEvent({
                (char)(int(Type::NoteOn) | (channel & 0xf)), 
                (char)(note & 0x7f), 
                (char)velocity
            });
        }
        static SimpleEvent NoteOff(int channel, int note, int velocity)
        {
            if(velocity < 0) velocity = 0;
            if(velocity > 127) velocity = 127;
            return SimpleEvent({
                (char)(int(Type::NoteOff) | (channel & 0xf)), 
                (char)(note & 0x7f), 
                (char)velocity
            });
        }
        static SimpleEvent ControlChange(int channel, int cc, int value)
        {
            if(value < 0) value = 0;
            if(value > 127) value = 127;
            return SimpleEvent({
                (char)(int(Type::ControlChange) | (channel & 0xf)), 
                (char)(cc & 0x7f), 
                (char)value
            });
        }
        static SimpleEvent ProgramChange(int channel, int p)
        {
            return SimpleEvent({
                (char)(int(Type::ProgramChange) | (channel & 0xf)), 
                (char)(p & 0x7f), 
                (char)0
            });
        }
        SimpleEvent ChangeChannel(int channel) const
        {
            return SimpleEvent({
                (char)(int(type()) | (channel & 0xf)), 
                m_Data[1], 
                m_Data[2]
            });
        }
        SimpleEvent AllNotesOff(int channel) const
        {
            return ControlChange(channel, 123, 0);
        }
        auto operator<=>(const SimpleEvent&) const = default;
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

    private:
        std::optional<SimpleEvent> m_Event;
        std::vector<char> m_SysexEvent;
    };
}