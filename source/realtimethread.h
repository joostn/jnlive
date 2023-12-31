#pragma once
#include "lilvjacklink.h"

namespace realtimethread
{
    constexpr int kMaxNumPlugins = 128;
    constexpr int kMaxNumMidiPorts = 128;
    class Data
    {
    public:
        class TPlugin
        {
        public:
            TPlugin() = default;
            TPlugin(lilvjacklink::LinkedPluginInstance *pluginInstance, float amplitudeFactor) : m_PluginInstance(pluginInstance), m_AmplitudeFactor(amplitudeFactor)
            {
            }
            lilvjacklink::LinkedPluginInstance& LinkedPluginInstance() const
            {
                return *m_LinkedPluginInstance;
            }
            float AmplitudeFactor() const
            {
                return m_AmplitudeFactor;
            }
            void SetAmplitudeFactor(float amplitudeFactor)
            {
                m_AmplitudeFactor = amplitudeFactor;
            }
        private:
            lilvjacklink::LinkedPluginInstance *m_LinkedPluginInstance = nullptr;
            float m_AmplitudeFactor = 0.0f;
        };
        class TMidiPort
        {
        public:
            TMidiPort() = default;
            TMidiPort(jack_port_t *port, int pluginIndex) : m_Port(port), m_PluginIndex(pluginIndex)
            {
            }
            jack_port_t& Port() const
            {
                return *m_Port;
            }
            int PluginIndex() const
            {
                return m_PluginIndex;
            }
            void SetPluginIndex(int pluginIndex)
            {
                m_PluginIndex = pluginIndex;
            }
        private:
            jack_port_t *m_Port = nullptr;
            int m_PluginIndex = -1;
        };
    public:
        TPlugin* Plugins()
        {
            return m_PluginStorage.data();
        }
        const TPlugin* Plugins() const
        {
            return m_PluginStorage.data();
        }
        size_t NumPlugins() const
        {
            return m_NumPlugins;
        }
        void SetNumPlugins(size_t numPlugins)
        {
            m_NumPlugins = numPlugins;
        }
        TMidiPort* MidiPorts()
        {
            return m_MidiPortStorage.data();
        }
        const TMidiPort* MidiPorts() const
        {
            return m_MidiPortStorage.data();
        }
        size_t NumMidiPorts() const
        {
            return m_NumMidiPorts;
        }
        void SetNumMidiPorts(size_t numMidiPorts)
        {
            m_NumMidiPorts = numMidiPorts;
        }
        
    private:
        std::array<TPlugin, kMaxNumPlugins> m_PluginStorage;
        size_t m_NumPlugins = 0;
        std::array<TMidiPort, kMaxNumMidiPorts> m_MidiPortStorage;
        size_t m_NumMidiPorts = 0;
        std::array<jack_port_t*, 2> m_OutputAudioPorts = {nullptr, nullptr};
    };
    class SetDataMessage : public ringbuf::PacketBase
    {
    public:
        SetDataMessage(Data &&data) : m_Data(data)
        {
        }
        const Data& Data() const
        {
            return m_Data;
        }
    private:
        Data m_Data;
    };
    class DeletePluginInstanceMessage : public ringbuf::PacketBase
    {
    public:
        DeletePluginInstanceMessage(lilvjacklink::LinkedPluginInstance *pluginInstance) : m_PluginInstance(pluginInstance)
        {
        }
        lilvjacklink::LinkedPluginInstance& PluginInstance() const
        {
            return *m_PluginInstance;
        }    
    private:
        lilvjacklink::LinkedPluginInstance *m_PluginInstance = nullptr;
    };
    class Processor
    {
    public:
        Processor(jack_nframes_t bufsize) : m_Bufsize(), m_AudioBufferStorage(2*bufsize)
        {
        }
        void Process(jack_nframes_t nframes)
        {
            if(nframes > m_Bufsize)
            {
                throw std::runtime_error("nframes > bufsize");
            }
            ProcessMessages();
            ProcessIncomingMidi(nframes);
            ProcessOutgoingAudio(nframes);

        }
    private:
        void ProcessMessages()
        {
            while(true)
            {
                auto message = m_IncomingRingBuf.Read();
                if(!message) break;
                if(auto setdatamessage = dynamic_cast<SetDataMessage*>(message))
                {
                    m_Data = setdatamessage->Data();
                }
                else if(auto deleteplugininstancemessage = dynamic_cast<DeletePluginInstanceMessage*>(message))
                {
                    // post back to main thread
                    m_OutgoingRingBuf.Write(*deleteplugininstancemessage);
                }
            }
        }
        void ProcessOutgoingAudio(jack_nframes_t nframes)
        {
            std::array<float*, 2> outputAudioPorts = {
                jack_port_get_buffer(m_Data.OutputAudioPorts()[0], nframes),
                jack_port_get_buffer(m_Data.OutputAudioPorts()[1], nframes)
            };
            if(m_Data.NumPlugins() == 0)
            {
                for(size_t i = 0; i < nframes; ++i)
                {
                    outputAudioPorts[0][i] = 0.0f;
                    outputAudioPorts[1][i] = 0.0f;
                }
            }
            else
            {
                std::array<float*, 2> accumulator = {m_AudioBufferStorage.data() + 0*nframes, m_AudioBufferStorage.data() + 1*nframes};
                for(size_t pluginindex = 0; pluginindex < m_Data.NumPlugins(); ++pluginindex)
                {
                    const auto &plugin = m_Data.Plugins()[pluginindex];
                    auto outputAudioPortIndices = plugin.LinkedPluginInstance().Instance().Plugin().OutputAudioPortIndices();
                    auto lilvinstance = plugin.LinkedPluginInstance().Instance().get();
                    lilv_instance_connect_port(lilvinstance, outputAudioPortIndices[0], outputAudioPorts[0]);
                    lilv_instance_connect_port(lilvinstance, outputAudioPortIndices[1], outputAudioPorts[1]);
                    lilv_instance_run(lilvinstance, nframes);
                    auto multiplier = plugin.AmplitudeFactor();
                    if(pluginindex == 0)
                    {
                        for(size_t i = 0; i < nframes; ++i)
                        {
                            accumulator[0][i] = multiplier * outputAudioPorts[0][i];
                            accumulator[1][i] = multiplier * outputAudioPorts[1][i];
                        }
                    }
                    else
                    {
                        for(size_t i = 0; i < nframes; ++i)
                        {
                            accumulator[0][i] += multiplier * outputAudioPorts[0][i];
                            accumulator[1][i] += multiplier * outputAudioPorts[1][i];
                        }
                    }
                }
            }
        }
        void ProcessIncomingMidi(jack_nframes_t nframes)
        {
            for(size_t midiportindex = 0; midiportindex < m_Data.NumMidiPorts(); ++midiportindex)
            {
                auto &midiport = m_Data.MidiPorts()[midiportindex];
                auto buf = jack_port_get_buffer(midiport.Port(), nframes);
                if(buf)
                {
                    LV2_Evbuf *evbuf = nullptr;
                    LV2_Evbuf_Iterator iter;
                    if( (midiport.PluginIndex() >= 0) && (midiport.PluginIndex() < m_Data.NumPlugins()) )
                    {
                        auto &plugin = m_Data.Plugins()[midiport.PluginIndex()];
                        auto &plugininstance = plugin.LinkedPluginInstance();
                        evbuf = plugininstance.Evbuf();
                        lv2_evbuf_reset(evbuf, true);
                        iter = lv2_evbuf_begin(evbuf);
                    }
                    void* buf = jack_port_get_buffer(m_JackPort->get(), nframes);
                    if(buf)
                    {
                        for (uint32_t i = 0; i < jack_midi_get_event_count(buf); ++i) 
                        {
                            jack_midi_event_t ev;
                            jack_midi_event_get(&ev, buf, i);
                            if(evbuf)
                            {
                                lv2_evbuf_write(&iter, ev.time, 0, m_UridMidiEvent, ev.size, ev.buffer);
                            }
                        }
                    }
                }
            }
        }

    private:
        TData m_Data;
        jack_nframes_t m_Bufsize;
        std::vector<float> m_AudioBufferStorage;
        ringbuf::RingBuf m_IncomingRingBuf(130000, 4096);
        ringbuf::RingBuf m_OutgoingRingBuf(130000, 4096);
    };
}