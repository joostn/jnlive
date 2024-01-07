#pragma once
#include "lilvutils.h"
#include "jackutils.h"
#include "ringbuf.h"
#include <jack/midiport.h>
#include "lv2/midi/midi.h"

namespace realtimethread
{
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
            auto operator<=>(const TMidiPort&) const = default;
        private:
            jack_port_t *m_Port = nullptr;
            int m_PluginIndex = -1;
        };
    public:
        Data() = default;
        Data(const std::vector<Plugin>& plugins, const std::vector<TMidiPort>& midiPorts, const std::array<jack_port_t*, 2>& outputAudioPorts) : m_Plugins(plugins), m_MidiPorts(midiPorts), m_OutputAudioPorts(outputAudioPorts) {}
        const std::vector<Plugin>& Plugins() const { return m_Plugins; }
        const std::vector<TMidiPort>& MidiPorts() const { return m_MidiPorts; }
        const std::array<jack_port_t*, 2>& OutputAudioPorts() const { return m_OutputAudioPorts; }
        auto operator<=>(const Data&) const = default;
        
    private:
        std::vector<Plugin> m_Plugins;
        std::vector<TMidiPort> m_MidiPorts;
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
    class ControlPortChangedMessage : public ringbuf::PacketBase
    {
    public:
        ControlPortChangedMessage(lilvutils::TConnection<lilvutils::TControlPort> *connection, float newValue) : m_Connection(connection), m_NewValue(newValue) {}
        lilvutils::TConnection<lilvutils::TControlPort> * Connection() const {return m_Connection;}
        float NewValue() const {return m_NewValue;}
    private:
        lilvutils::TConnection<lilvutils::TControlPort> *m_Connection;
        float m_NewValue;
    };
    class AtomPortEventMessage : public ringbuf::PacketBase
    {
    public:
        AtomPortEventMessage(lilvutils::TConnection<lilvutils::TAtomPort>* connection, uint32_t frames, uint32_t subframes, LV2_URID type, uint32_t size, const void* body) : ringbuf::PacketBase(size, body), m_Connection(connection), m_Frames(frames), m_SubFrames(subframes), m_Type(type)
        {
        }
        uint32_t Frames() const { return m_Frames; }
        uint32_t SubFrames() const { return m_SubFrames; }
        LV2_URID Type() const { return m_Type; }
        lilvutils::TConnection<lilvutils::TAtomPort>* Connection() const { return m_Connection; }
    private:
        uint32_t m_Frames;
        uint32_t m_SubFrames;
        LV2_URID m_Type;
        lilvutils::TConnection<lilvutils::TAtomPort>* m_Connection;
    };
    class AsyncFunctionMessage : public ringbuf::PacketBase
    {
        /*
        A message used to store a function which will be called later. The Call() method must eventually be called, otherwise we will leak memory.
        The intended purpose is to defer deletion of objects until we are sure they are no longer used by the realtime thread.
        If an object is scheduled for deletion, the main thread first posts a message to the realtime thread with the modified data structures (which no longer refer to the deleted object).
        Then the main thread posts an AsyncFunctionMessage with a function that actually deletes the object.
        The realtime thread, at the end of its run, posts back all AsyncFunctionMessages to the main thread. We can be sure that the realtime thread will no longer access the deleted objects. The main thread then executes the function, which deletes the object.
        */
    public:
        AsyncFunctionMessage() : m_Function(nullptr) {}
        AsyncFunctionMessage(AsyncFunctionMessage &&src)
        {
            if(&src != this)
            {
                m_Function = src.m_Function;
                src.m_Function = nullptr;
            }
        }
        AsyncFunctionMessage& operator=(AsyncFunctionMessage &&src)
        {
            if(&src != this)
            {
                m_Function = src.m_Function;
                src.m_Function = nullptr;
            }
            return *this;
        }
        AsyncFunctionMessage(std::function<void()> &&function) : m_Function(new std::function<void()>(std::move(function))) {}
        void Call() const
        {
            if(m_Function)
            {
                (*m_Function)();
                delete m_Function;
            }
        }
        AsyncFunctionMessage Clone()const
        {
            // careful! Only use this if we can guarantee that this->Call() will no longer be called.
            AsyncFunctionMessage result;
            result.m_Function = m_Function;
            return result;
        }
    private:
        std::function<void()> *m_Function;
    };
    class Processor
    {
    public:
        Processor(jack_nframes_t bufsize) : m_Bufsize(bufsize), m_DataInRtThread(m_CurrentData.get())
        {
          m_UridMidiEvent = lilvutils::World::Static().UriMapLookup(LV2_MIDI__MidiEvent);
          m_RealtimeThreadInterface.SendAtomPortEventFunc = [this](lilvutils::TConnection<lilvutils::TAtomPort>* connection, uint32_t frames, uint32_t subframes, LV2_URID type, uint32_t size, const void* body)
          {
              SendAtomPortEventFromMainThread(connection, frames, subframes, type, size, body);
          };
          m_RealtimeThreadInterface.SendControlValueFunc = [this](lilvutils::TConnection<lilvutils::TControlPort>* connection, float value)
          {
              SendControlValueFromMainThread(connection, value);
          };
        }
        void Process(jack_nframes_t nframes)
        {
            if(!m_DataInRtThread) return;
            if(nframes > m_Bufsize)
            {
                throw std::runtime_error("nframes > bufsize");
            }
            ResetEvBufs();
            ProcessMessagesInRealtimeThread();
            ProcessIncomingMidi(nframes);
            ProcessOutgoingAudio(nframes);
            ProcessOutputPorts();
            SendPendingAsyncFunctionMessages();
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
        void SendAtomPortEventFromMainThread(lilvutils::TConnection<lilvutils::TAtomPort>* connection, uint32_t frames, uint32_t subframes, LV2_URID type, uint32_t size, const void* body)
        {
            RingBufToRtThread().Write(realtimethread::AtomPortEventMessage(connection, frames, subframes, type, size, body));
        }
        void SendControlValueFromMainThread(lilvutils::TConnection<lilvutils::TControlPort>* connection, float value)
        {
            RingBufToRtThread().Write(realtimethread::ControlPortChangedMessage(connection, value));
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
                if(auto asyncfunctionmessage = dynamic_cast<const realtimethread::AsyncFunctionMessage*>(message); asyncfunctionmessage)
                {
                    asyncfunctionmessage->Call();
                }
                else if(auto cpchangedmessage = dynamic_cast<const realtimethread::ControlPortChangedMessage*>(message); cpchangedmessage)
                {
                    cpchangedmessage->Connection()->SetValueInMainThread(cpchangedmessage->NewValue(), true);
                }
                else if(auto atomPortEventMessage = dynamic_cast<const AtomPortEventMessage*>(message))
                {
                    atomPortEventMessage->Connection()->instance().OnAtomPortMessage(*atomPortEventMessage->Connection(), atomPortEventMessage->Frames(), atomPortEventMessage->SubFrames(), atomPortEventMessage->Type(), atomPortEventMessage->AdditionalDataSize(), atomPortEventMessage->AdditionalDataBuf());
                }
            }
        }
        const lilvutils::RealtimeThreadInterface& realtimeThreadInterface() const { return m_RealtimeThreadInterface; }
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
                    if(m_NumStoredAsyncFunctionMessages >= m_BufferForAsyncFunctionMessages.size())
                    {
                        throw std::runtime_error("m_BufferForAsyncFunctionMessages overrun");
                    }
                    m_BufferForAsyncFunctionMessages[m_NumStoredAsyncFunctionMessages++] = std::move(asyncfunctionmessage->Clone());
                }
                else if(auto cpchangedmessage = dynamic_cast<const realtimethread::ControlPortChangedMessage*>(message); cpchangedmessage)
                {
                    *cpchangedmessage->Connection()->Buffer() = cpchangedmessage->NewValue();
                }
                else if(auto atomPortEventMessage = dynamic_cast<const AtomPortEventMessage*>(message))
                {
                    auto &bufferIterator = atomPortEventMessage->Connection()->BufferIterator();
                    lv2_evbuf_write(&bufferIterator, atomPortEventMessage->Frames(), atomPortEventMessage->SubFrames(), atomPortEventMessage->Type(), atomPortEventMessage->AdditionalDataSize(), atomPortEventMessage->AdditionalDataBuf());
                }                
            }
        }
        void SendPendingAsyncFunctionMessages()
        {
            for(size_t i=0; i < m_NumStoredAsyncFunctionMessages; ++i)
            {
                // post back to main thread
                m_RingBufFromRtThread.Write(m_BufferForAsyncFunctionMessages[i]);
            }
            m_NumStoredAsyncFunctionMessages = 0;
        }
        void ResetEvBufs()
        {
            const auto &data = *m_DataInRtThread;
            for(const auto &plugin: data.Plugins())
            {
                auto &instance = plugin.PluginInstance();
                for(auto &connection: instance.Connections())
                {
                    if(auto atomportconnection = dynamic_cast<lilvutils::TConnection<lilvutils::TAtomPort>*>(connection.get()); atomportconnection)
                    {
                        bool isinput = atomportconnection->Port().Direction() == lilvutils::TPortBase::TDirection::Input;
                        lv2_evbuf_reset(atomportconnection->Buffer(), isinput);
                        atomportconnection->BufferIterator() = lv2_evbuf_begin(atomportconnection->Buffer());
                    }
                }
            }
        }
        void ProcessOutputPorts()
        {
            const auto &data = *m_DataInRtThread;
            for(const auto &plugin: data.Plugins())
            {
                auto &instance = plugin.PluginInstance();
                for(auto &connection: instance.Connections())
                {
                    if(auto controlportconnection = dynamic_cast<lilvutils::TConnection<lilvutils::TControlPort>*>(connection.get()); controlportconnection)
                    {
                        const auto &port = controlportconnection->Port();
                        if(port.Direction() == lilvutils::TPortBase::TDirection::Output)
                        {
                            if(*controlportconnection->Buffer() != controlportconnection->OrigValue())
                            {
                                controlportconnection->OrigValue() = *controlportconnection->Buffer();
                                m_RingBufFromRtThread.Write(ControlPortChangedMessage(controlportconnection, *controlportconnection->Buffer()));
                            }
                        }
                    }
                    else if(auto atomportconnection = dynamic_cast<lilvutils::TConnection<lilvutils::TAtomPort>*>(connection.get()); atomportconnection)
                    {
                        const auto &port = controlportconnection->Port();
                        if(port.Direction() == lilvutils::TPortBase::TDirection::Output)
                        {
                            while(lv2_evbuf_is_valid(atomportconnection->BufferIterator()))
                            {
                                // Get event from LV2 buffer
                                uint32_t frames    = 0;
                                uint32_t subframes = 0;
                                LV2_URID type      = 0;
                                uint32_t size      = 0;
                                void*    body      = NULL;
                                lv2_evbuf_get(atomportconnection->BufferIterator(), &frames, &subframes, &type, &size, &body);
                                m_RingBufFromRtThread.Write(AtomPortEventMessage(atomportconnection, frames, subframes, type, size, body));
                                atomportconnection->BufferIterator() = lv2_evbuf_next(atomportconnection->BufferIterator());
                            }
                        }
                    }
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
            for(const auto &plugin: data.Plugins())
            {
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
            for(const auto &midiport: data.MidiPorts())
            {
                auto buf = jack_port_get_buffer(&midiport.Port(), nframes);
                auto evtcount = jack_midi_get_event_count(buf);
                if(buf && (evtcount > 0))
                {
                    auto pluginindex = midiport.PluginIndex();
                    if( (pluginindex >= 0) && (pluginindex < (int)data.Plugins().size()) )
                    {
                        auto &plugin = data.Plugins()[midiport.PluginIndex()];
                        auto &plugininstance = plugin.PluginInstance();
                        auto midiinputindexOrNull = plugininstance.plugin().MidiInputIndex();
                        if(midiinputindexOrNull)
                        {
                            if(auto atomconnection = dynamic_cast<lilvutils::TConnection<lilvutils::TAtomPort>*>(plugininstance.Connections().at(*midiinputindexOrNull).get()))
                            {
                                for (uint32_t i = 0; i < jack_midi_get_event_count(buf); ++i) 
                                {
                                    jack_midi_event_t ev;
                                    jack_midi_event_get(&ev, buf, i);
                                    lv2_evbuf_write(&atomconnection->BufferIterator(), ev.time, 0, m_UridMidiEvent, ev.size, ev.buffer);
                                }
                            }
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
        std::array<AsyncFunctionMessage, 400> m_BufferForAsyncFunctionMessages;
        size_t m_NumStoredAsyncFunctionMessages = 0;
        lilvutils::RealtimeThreadInterface m_RealtimeThreadInterface;
    };
}