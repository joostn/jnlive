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
        const std::array<jack_port_t*, 2>& OutputAudioPorts() const { return m_OutputAudioPorts; }
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
        SetDataMessage(const Data *data) : m_Data(data) {}
        const Data* data() const { return m_Data; }
    private:
        const Data *m_Data;
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
        Processor(jack_nframes_t bufsize) : m_Bufsize(bufsize), m_CurrentData(std::make_unique<Data>()), m_DataInRtThread(m_CurrentData.get())
        {
          m_UridMidiEvent = lilvutils::World::Static().UriMapLookup(LV2_MIDI__MidiEvent);
        }
        void Process(jack_nframes_t nframes)
        {
            if(nframes > m_Bufsize)
            {
                throw std::runtime_error("nframes > bufsize");
            }
            ProcessMessagesInRealtimeThread();
            ProcessIncomingMidi(nframes);
            ProcessOutgoingAudio(nframes);
        }
        void SetDataFromMainThread(Data &&data)
        {
            auto olddata = std::move(m_CurrentData);
            m_CurrentData = std::make_unique<Data>(std::move(data));
            RingBufToRtThread().Write(SetDataMessage(m_CurrentData.get()));
            auto ptr = olddata.release();
            DeferredExecuteAfterRoundTrip([ptr](){
                delete ptr;
            });
        }
        void DeferredExecuteAfterRoundTrip(std::function<void()> &&function)
        {
            RingBufToRtThread().Write(realtimethread::AsyncFunctionMessage(std::move(function)));
        }
        ringbuf::RingBuf& RingBufToRtThread() { return m_RingBufToRtThread; }
        ringbuf::RingBuf& RingBufFromRtThread() { return m_RingBufFromRtThread; }
        void ProcessMessagesInMainThread() // should be called regularly
        {
            while(true)
            {
                auto message = RingBufFromRtThread().Read();
                if(!message) break;
                if(auto asyncfunctionmessage = dynamic_cast<const realtimethread::AsyncFunctionMessage*>(message))
                {
                    asyncfunctionmessage->Call();
                }
            }
        }
    private:
        void ProcessMessagesInRealtimeThread()
        {
            while(true)
            {
                auto message = RingBufToRtThread().Read();
                if(!message) break;
                if(auto setdatamessage = dynamic_cast<const SetDataMessage*>(message))
                {
                    m_DataInRtThread = setdatamessage->data();
                }
                else if(auto asyncfunctionmessage = dynamic_cast<const AsyncFunctionMessage*>(message))
                {
                    // post back to main thread
                    m_RingBufFromRtThread.Write(*asyncfunctionmessage);
                }
            }
        }
        void ProcessOutgoingAudio(jack_nframes_t nframes)
        {
            const auto &data = *m_DataInRtThread;
            std::array<float*, 2> mixedAudioPorts = {
                (float*)jack_port_get_buffer(data.OutputAudioPorts()[0], nframes),
                (float*)jack_port_get_buffer(data.OutputAudioPorts()[1], nframes)
            };
            bool hasoutputaudio = mixedAudioPorts[0] && mixedAudioPorts[1];
            if(hasoutputaudio)
            {
                for(size_t i = 0; i < nframes; ++i)
                {
                    mixedAudioPorts[0][i] = 0.0f;
                    mixedAudioPorts[1][i] = 0.0f;
                }
            }
            for(size_t pluginindex = 0; pluginindex < data.NumPlugins(); ++pluginindex)
            {
                const auto &plugin = data.Plugins()[pluginindex];
                auto outputAudioPortIndices = plugin.PluginInstance().plugin().OutputAudioPortIndices();
                auto lilvinstance = plugin.PluginInstance().get();
                plugin.PluginInstance().Run(nframes);
                auto multiplier = plugin.AmplitudeFactor();
                if(hasoutputaudio && outputAudioPortIndices[0] && outputAudioPortIndices[1])
                {
                    std::array<float*, 2> pluginOutputAudioPorts {
                        dynamic_cast<lilvutils::TConnection<lilvutils::TAudioPort>*>(plugin.PluginInstance().Connections().at(*outputAudioPortIndices[0]).get())->Buffer(),
                        dynamic_cast<lilvutils::TConnection<lilvutils::TAudioPort>*>(plugin.PluginInstance().Connections().at(*outputAudioPortIndices[1]).get())->Buffer()
                    };
                    for(size_t i = 0; i < nframes; ++i)
                    {
                        mixedAudioPorts[0][i] += multiplier * pluginOutputAudioPorts[0][i];
                        mixedAudioPorts[1][i] += multiplier * pluginOutputAudioPorts[1][i];
                    }
                }
            }
        }
        void ProcessIncomingMidi(jack_nframes_t nframes)
        {
            const auto &data = *m_DataInRtThread;
            for(size_t midiportindex = 0; midiportindex < data.NumMidiPorts(); ++midiportindex)
            {
                auto &midiport = data.MidiPorts()[midiportindex];
                auto buf = jack_port_get_buffer(&midiport.Port(), nframes);
                auto evtcount = jack_midi_get_event_count(buf);
                if(buf && (evtcount > 0))
                {
                    LV2_Evbuf *evbuf = nullptr;
                    LV2_Evbuf_Iterator iter;
                    if( (midiport.PluginIndex() >= 0) && (midiport.PluginIndex() < data.NumPlugins()) )
                    {
                        auto &plugin = data.Plugins()[midiport.PluginIndex()];
                        auto &plugininstance = plugin.PluginInstance();
                        evbuf = plugininstance.MidiInEvBuf();
                        iter = lv2_evbuf_begin(evbuf);
                    }
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

    private:
        std::unique_ptr<Data> m_CurrentData;
        const Data* m_DataInRtThread;
        jack_nframes_t m_Bufsize;
        ringbuf::RingBuf m_RingBufToRtThread {130000, 4096};
        ringbuf::RingBuf m_RingBufFromRtThread {130000, 4096};
        LV2_URID m_UridMidiEvent;
    };
}