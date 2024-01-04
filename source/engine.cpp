#include "engine.h"

namespace engine
{
    void Engine::ApplyJackConnections(bool forceNow)
    {
        auto now = std::chrono::steady_clock::now();
        if(m_LastApplyJackConnectionTime && !forceNow)
        {
            if(now - *m_LastApplyJackConnectionTime < std::chrono::milliseconds(1000))
            {
                return;
            }
        }
        m_LastApplyJackConnectionTime = now;
        size_t partindex = 0;
        for(const auto &midiInPortName: m_JackConnections.MidiInputs())
        {
            if(partindex >= m_Parts.size())
            {
                break;
            }
            if(!midiInPortName.empty())
            {
                const auto &part = m_Parts[partindex];
                part.MidiInPort()->LinkToPortByName(midiInPortName);
            }
        }
        for(size_t portindex: {0,1})
        {
            const auto &audioportname = m_JackConnections.AudioOutputs()[portindex];
            if(!audioportname.empty())
            {
                m_AudioOutPorts[portindex].LinkToPortByName(audioportname);
            }
        }

        
    }

}