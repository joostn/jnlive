#include "midi.h"
#include <sstream>

namespace midi {
    bool SimpleEvent::IsSupported(const void *buf, size_t size)
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
    SimpleEvent::SimpleEvent(const void *buf, size_t size)
    {
        if(!IsSupported(buf, size))
        {
            throw std::runtime_error("midi event not supported");
        }
        std::copy((const char*)buf, (const char*)buf + size, m_Data.begin());
        // if( (Type() == Type::NoteOn) && (Velocity() == 0) )
        // {
        //     // NoteOn with velocity 0 is actually a NoteOff
        //     m_Data[0] = (char)(int(Type::NoteOff) | (Channel() & 0x0f));
        // }
    }
    size_t SimpleEvent::ExpectedSize(char byte0)
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
    std::optional<SimpleEvent> SimpleEvent::Transpose(int delta) const
    {
        if( (type() == Type::NoteOn || type() == Type::NoteOff) && (delta != 0) )
        {
            int note = m_Data[1] + delta;
            if( (note >= 0) && (note <= 127) )
            {
                return SimpleEvent({
                    m_Data[0], 
                    (char)note, 
                    m_Data[2]
                });
            }
            else
            {
                return std::nullopt;
            }
        }
        return *this;
    }
    SimpleEvent SimpleEvent::NoteOn(int channel, int note, int velocity)
    {
        if(velocity < 1) velocity = 1;
        if(velocity > 127) velocity = 127;
        return SimpleEvent({
            (char)(int(Type::NoteOn) | (channel & 0xf)), 
            (char)(note & 0x7f), 
            (char)velocity
        });
    }
    SimpleEvent SimpleEvent::NoteOff(int channel, int note, int velocity)
    {
        if(velocity < 0) velocity = 0;
        if(velocity > 127) velocity = 127;
        return SimpleEvent({
            (char)(int(Type::NoteOff) | (channel & 0xf)), 
            (char)(note & 0x7f), 
            (char)velocity
        });
    }
    SimpleEvent SimpleEvent::ControlChange(int channel, int cc, int value)
    {
        if(value < 0) value = 0;
        if(value > 127) value = 127;
        return SimpleEvent({
            (char)(int(Type::ControlChange) | (channel & 0xf)), 
            (char)(cc & 0x7f), 
            (char)value
        });
    }
    SimpleEvent SimpleEvent::ProgramChange(int channel, int p)
    {
        return SimpleEvent({
            (char)(int(Type::ProgramChange) | (channel & 0xf)), 
            (char)(p & 0x7f), 
            (char)0
        });
    }
    SimpleEvent SimpleEvent::ChangeChannel(int channel) const
    {
        return SimpleEvent({
            (char)(int(type()) | (channel & 0xf)), 
            m_Data[1], 
            m_Data[2]
        });
    }
    SimpleEvent SimpleEvent::AllNotesOff(int channel)
    {
        return ControlChange(channel, 123, 0);
    }
    void SimpleEvent::ToDebugStream(std::ostream &str) const
    {
        str << "midi::SimpleEvent(";
        switch(type())
        {
            case Type::NoteOff: str << "NoteOff"; break;
            case Type::NoteOn: str << "NoteOn"; break;
            case Type::PolyphonicKeyPressure: str << "PolyphonicKeyPressure"; break;
            case Type::ControlChange: str << "ControlChange"; break;
            case Type::ProgramChange: str << "ProgramChange"; break;
            case Type::ChannelPressure: str << "ChannelPressure"; break;
            case Type::PitchBendChange: str << "PitchBendChange"; break;
            case Type::System: str << "System"; break;
        }
        str << ", channel=" << Channel();
        str << ", data=";
        for(size_t i = 1; i < Size(); ++i)
        {
            str << (int)m_Data[i] << " ";
        }
        str << ")";
    }
    std::string SimpleEvent::ToDebugString() const
    {
        std::stringstream str;
        ToDebugStream(str);
        return str.str();
    }

    void TMidiOrSysexEvent::ToDebugStream(std::ostream &str) const
    {
        if(m_Event)
        {
            m_Event->ToDebugStream(str);
        }
        else
        {
            str << "sysex (" << m_SysexEvent.size() << " bytes)";
        }
    }
    std::string TMidiOrSysexEvent::ToDebugString() const
    {
        std::stringstream str;
        ToDebugStream(str);
        return str.str();
    }

}// namespace midi