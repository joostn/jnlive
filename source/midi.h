#pragma once
#include <array>
#include <algorithm>
#include <stdexcept>

namespace midi
{
    class Event
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
            auto type = (Type)(((const char*)buf)[0] & 0xF0);
            if(type < Type::NoteOff || type >= Type::System)
            {
                return false;
            }
            size_t expectedsize = (type == Type::ProgramChange || type == Type::ChannelPressure) ? 2 : 3;
            if(size != expectedsize)
            {
                return false;
            }
            return true;
        }
        Event(const void *buf, size_t size)
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
        size_t Size() const { return (type() == Type::ProgramChange || type() == Type::ChannelPressure) ? 2 : 3; }
        static Event NoteOn(int channel, int note, int velocity)
        {
            if(velocity < 1) velocity = 1;
            if(velocity > 127) velocity = 127;
            return Event({
                (char)(int(Type::NoteOn) | (channel & 0xf)), 
                (char)(note & 0x7f), 
                (char)velocity
            });
        }
        static Event NoteOff(int channel, int note, int velocity)
        {
            if(velocity < 0) velocity = 0;
            if(velocity > 127) velocity = 127;
            return Event({
                (char)(int(Type::NoteOff) | (channel & 0xf)), 
                (char)(note & 0x7f), 
                (char)velocity
            });
        }
        static Event ControlChange(int channel, int cc, int value)
        {
            if(value < 0) value = 0;
            if(value > 127) value = 127;
            return Event({
                (char)(int(Type::ControlChange) | (channel & 0xf)), 
                (char)(cc & 0x7f), 
                (char)value
            });
        }
        static Event ProgramChange(int channel, int p)
        {
            return Event({
                (char)(int(Type::ProgramChange) | (channel & 0xf)), 
                (char)(p & 0x7f), 
                (char)0
            });
        }
        Event ChangeChannel(int channel) const
        {
            return Event({
                (char)(int(type()) | (channel & 0xf)), 
                m_Data[1], 
                m_Data[2]
            });
        }
        Event AllNotesOff(int channel) const
        {
            return ControlChange(channel, 123, 0);
        }
        auto operator<=>(const Event&) const = default;
    private:
        Event(const std::array<char, 3> &data) : m_Data(data) {}

    private:
        std::array<char, 3> m_Data {0,0,0};
    };
}