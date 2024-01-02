#pragma once
#include "lilvutils.h"
#include "jackutils.h"
#include "ringbuf.h"
#include <jack/midiport.h>
#include "lv2/midi/midi.h"

namespace realtimethread
{
    constexpr int kMaxNumPlugins = 128;
    constexpr int kMaxNumMidiPorts = 8;
    class Data
    {
    public:
        class Plugin
        {
        public:
            Plugin() = default;
            Plugin(lilvutils::Instance *pluginInstance, float amplitudeFactor) : m_PluginInstance(pluginInstance), m_AmplitudeFactor(amplitudeFactor)
            {
            }
            lilvutils::Instance& PluginInstance() const
            {
                return *m_PluginInstance;
            }
            float AmplitudeFactor() const
            {
                return m_AmplitudeFactor;
            }
            void SetAmplitudeFactor(float amplitudeFactor)
            {
                m_AmplitudeFactor = amplitudeFactor;
            }
            auto operator<=>(const Plugin&) const = default;
        private:
            lilvutils::Instance *m_PluginInstance = nullptr;
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
            auto operator<=>(const TMidiPort&) const = default;
        private:
            jack_port_t *m_Port = nullptr;
            int m_PluginIndex = -1;
        };
    public:
        Plugin* Plugins()
        {
            return m_PluginStorage.data();
        }
        const Plugin* Plugins() const
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
        std::array<jack_port_t*, 2>& OutputAudioPorts() { return m_OutputAudioPorts; }
        auto operator<=>(const Data&) const = default;
        
    private:
        std::array<Plugin, kMaxNumPlugins> m_PluginStorage;
        size_t m_NumPlugins = 0;
        std::array<TMidiPort, kMaxNumMidiPorts> m_MidiPortStorage;
        size_t m_NumMidiPorts = 0;
        std::array<jack_port_t*, 2> m_OutputAudioPorts = {nullptr, nullptr};
    };
    class SetDataMessage : public ringbuf::PacketBase
    {
    public:
        SetDataMessage(Data &&data) : m_Data(data) {}
        const Data& data() const { return m_Data; }
    private:
        Data m_Data;
    };
    class AsyncFunctionMessage : public ringbuf::PacketBase
    {
        /*
        A message used to store a function which will be called later. The Call() method must eventually be called, otherwise we will leak memory.
        */
    public:
        AsyncFunctionMessage(std::function<void()> &&function) : m_Function(new std::function<void()>(std::move(function))) {}
        void Call() const
        {
            (*m_Function)();
            delete m_Function;
        }
    private:
        std::function<void()> *m_Function;
    };
    class Processor
    {
    public:
        Processor(jack_nframes_t bufsize) : m_Bufsize(bufsize), m_AudioBufferStorage(2*bufsize)
        {
          m_UridMidiEvent = lilvutils::World::Static().UriMapLookup(LV2_MIDI__MidiEvent);
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
        void SetDataFromMainThread(Data &&data)
        {
            IncomingRingBuf().Write(SetDataMessage(std::move(data)));
        }
        void DeferredExecuteAfterRoundTrip(std::function<void()> &&function)
        {
            OutgoingRingBuf().Write(realtimethread::AsyncFunctionMessage(std::move(function)));
        }
        ringbuf::RingBuf& IncomingRingBuf() { return m_IncomingRingBuf; }
        ringbuf::RingBuf& OutgoingRingBuf() { return m_OutgoingRingBuf; }
        
    private:
        void ProcessMessages()
        {
            while(true)
            {
                auto message = IncomingRingBuf().Read();
                if(!message) break;
                if(auto setdatamessage = dynamic_cast<const SetDataMessage*>(message))
                {
                    m_Data = setdatamessage->data();
                }
                else if(auto asyncfunctionmessage = dynamic_cast<const AsyncFunctionMessage*>(message))
                {
                    // post back to main thread
                    m_OutgoingRingBuf.Write(*asyncfunctionmessage);
                }
            }
        }
        void ProcessOutgoingAudio(jack_nframes_t nframes)
        {
            std::array<float*, 2> outputAudioPorts = {
                (float*)jack_port_get_buffer(m_Data.OutputAudioPorts()[0], nframes),
                (float*)jack_port_get_buffer(m_Data.OutputAudioPorts()[1], nframes)
            };
            bool hasoutputaudio = outputAudioPorts[0] && outputAudioPorts[1];
            if(m_Data.NumPlugins() == 0)
            {
                if(hasoutputaudio)
                {
                    for(size_t i = 0; i < nframes; ++i)
                    {
                        outputAudioPorts[0][i] = 0.0f;
                        outputAudioPorts[1][i] = 0.0f;
                    }
                }
            }
            else
            {
                std::array<float*, 2> pluginaudiobufs = {m_AudioBufferStorage.data() + 0*nframes, m_AudioBufferStorage.data() + 1*nframes};
                for(size_t pluginindex = 0; pluginindex < m_Data.NumPlugins(); ++pluginindex)
                {
                    const auto &plugin = m_Data.Plugins()[pluginindex];
                    auto outputAudioPortIndices = plugin.PluginInstance().plugin().OutputAudioPortIndices();
                    auto lilvinstance = plugin.PluginInstance().get();
                    if(outputAudioPortIndices[0])
                    {
                        lilv_instance_connect_port(lilvinstance, outputAudioPortIndices[0].value(), pluginaudiobufs[0]);
                    }
                    if(outputAudioPortIndices[1])
                    {
                        lilv_instance_connect_port(lilvinstance, outputAudioPortIndices[1].value(), pluginaudiobufs[1]);
                    }
                    plugin.PluginInstance().Run(nframes);
                    auto multiplier = plugin.AmplitudeFactor();
                    if(hasoutputaudio)
                    {
                        if(pluginindex == 0)
                        {
                            for(size_t i = 0; i < nframes; ++i)
                            {
                                outputAudioPorts[0][i] = multiplier * pluginaudiobufs[0][i];
                                outputAudioPorts[1][i] = multiplier * pluginaudiobufs[1][i];
                            }
                        }
                        else
                        {
                            for(size_t i = 0; i < nframes; ++i)
                            {
                                outputAudioPorts[0][i] += multiplier * pluginaudiobufs[0][i];
                                outputAudioPorts[1][i] += multiplier * pluginaudiobufs[1][i];
                            }
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
                auto buf = jack_port_get_buffer(&midiport.Port(), nframes);
                if(buf)
                {
                    LV2_Evbuf *evbuf = nullptr;
                    LV2_Evbuf_Iterator iter;
                    if( (midiport.PluginIndex() >= 0) && (midiport.PluginIndex() < m_Data.NumPlugins()) )
                    {
                        auto &plugin = m_Data.Plugins()[midiport.PluginIndex()];
                        auto &plugininstance = plugin.PluginInstance();
                        evbuf = plugininstance.MidiInEvBuf();
                        lv2_evbuf_reset(evbuf, true);
                        iter = lv2_evbuf_begin(evbuf);
                    }
                    void* buf = jack_port_get_buffer(&midiport.Port(), nframes);
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
        Data m_Data;
        jack_nframes_t m_Bufsize;
        std::vector<float> m_AudioBufferStorage;
        ringbuf::RingBuf m_IncomingRingBuf {130000, 4096};
        ringbuf::RingBuf m_OutgoingRingBuf {130000, 4096};
        LV2_URID m_UridMidiEvent;
    };
}