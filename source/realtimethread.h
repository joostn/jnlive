#pragma once
#include "lilvutils.h"
#include "jackutils.h"
#include "ringbuf.h"
#include <jack/midiport.h>
#include "lv2/midi/midi.h"
#include "midi.h"

namespace realtimethread
{
    class Data
    {
    public:
        class Plugin
        {
        public:
            Plugin() = default;
            Plugin(lilvutils::Instance *pluginInstance, float amplitudeFactor, bool doOverrideChannel, LV2_Evbuf_Iterator *midiInBuf) : m_PluginInstance(pluginInstance), m_AmplitudeFactor(amplitudeFactor), m_DoOverrideChannel(doOverrideChannel), m_MidiInBuf(midiInBuf)
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
            bool DoOverrideChannel() const
            {
                return m_DoOverrideChannel;
            }
            LV2_Evbuf_Iterator *MidiInBuf() const
            {
                return m_MidiInBuf;
            }
            auto operator<=>(const Plugin&) const = default;
        private:
            lilvutils::Instance *m_PluginInstance = nullptr;
            float m_AmplitudeFactor = 0.0f;
            bool m_DoOverrideChannel;
            LV2_Evbuf_Iterator *m_MidiInBuf;
        };
        class TMidiKeyboardPort
        {
        public:
            TMidiKeyboardPort() = default;
            TMidiKeyboardPort(jack_port_t *port, std::optional<int> pluginIndex, int overrideChannel) : m_Port(port), m_PluginIndex(pluginIndex), m_OverrideChannel(overrideChannel)
            {
            }
            jack_port_t& Port() const
            {
                return *m_Port;
            }
            std::optional<int> PluginIndex() const
            {
                return m_PluginIndex;
            }
            int OverrideChannel() const
            {
                return m_OverrideChannel;
            }
            auto operator<=>(const TMidiKeyboardPort&) const = default;
        private:
            jack_port_t *m_Port = nullptr;
            std::optional<int> m_PluginIndex;
            int m_OverrideChannel;
        };
        class TMidiAuxInPort
        {
            // callback will be called in main thread, not in audio thread!
        public:
            using TCallback = std::function<void(const midi::TMidiOrSysexEvent &event)>;
            TMidiAuxInPort(jack_port_t *port, const TCallback  *callback) : m_Port(port), m_Callback(std::move(callback)) {}
            auto operator<=>(const TMidiAuxInPort&) const = default;
            jack_port_t* Port() const { return m_Port; }
            const TCallback* Callback() const { return m_Callback; }

        private:
            jack_port_t *m_Port = nullptr;
            const TCallback *m_Callback;
        };
        class TMidiAuxOutPort
        {
            // callback will be called in main thread, not in audio thread!
        public:
            TMidiAuxOutPort(jack_port_t *port) : m_Port(port) {}
            auto operator<=>(const TMidiAuxOutPort&) const = default;
            jack_port_t* Port() const { return m_Port; }

        private:
            jack_port_t *m_Port = nullptr;
        };

    public:
        Data() = default;
        Data(const std::vector<Plugin>& plugins, const std::vector<TMidiKeyboardPort>& midiPorts, const std::vector<TMidiAuxInPort> &midiAuxInPorts, const std::vector<TMidiAuxOutPort> &midiAuxOutPorts, const std::array<jack_port_t*, 2>& outputAudioPorts, lilvutils::Instance *reverbInstance,
        float reverbLevel) : m_Plugins(plugins), m_MidiPorts(midiPorts), m_OutputAudioPorts(outputAudioPorts), m_MidiAuxInPorts(midiAuxInPorts), m_MidiAuxOutPorts(midiAuxOutPorts), m_ReverbInstance(reverbInstance), m_ReverbLevel(reverbLevel) {}
        const std::vector<Plugin>& Plugins() const { return m_Plugins; }
        const std::vector<TMidiKeyboardPort>& MidiPorts() const { return m_MidiPorts; }
        const std::array<jack_port_t*, 2>& OutputAudioPorts() const { return m_OutputAudioPorts; }
        const std::vector<TMidiAuxInPort>& MidiAuxInPorts() const { return m_MidiAuxInPorts; }
        const std::vector<TMidiAuxOutPort>& MidiAuxOutPorts() const { return m_MidiAuxOutPorts; }
        auto operator<=>(const Data&) const = default;
        lilvutils::Instance *ReverbInstance() const { return m_ReverbInstance; }
        float ReverbLevel() const { return m_ReverbLevel; }
        
    private:
        std::vector<Plugin> m_Plugins;
        std::vector<TMidiKeyboardPort> m_MidiPorts;
        std::vector<TMidiAuxInPort> m_MidiAuxInPorts;
        std::vector<TMidiAuxOutPort> m_MidiAuxOutPorts;
        std::array<jack_port_t*, 2> m_OutputAudioPorts = {nullptr, nullptr};
        lilvutils::Instance *m_ReverbInstance = nullptr;
        float m_ReverbLevel = 0.0f;
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
        The realtime thread, at the end of its run, echoes back all received AsyncFunctionMessages to the main thread. At this time we can be sure that the realtime thread will no longer access the deleted objects. The main thread then receives the AsyncFunctionMessage and executes the function, which deletes the object.
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
    class TMidiMessageToPlugin : public ringbuf::PacketBase
    {
    public:
        TMidiMessageToPlugin(const void *data, size_t size, LV2_Evbuf_Iterator* destinationPort) : ringbuf::PacketBase(size, data), m_DestinationPort(destinationPort) {}
        LV2_Evbuf_Iterator* DestinationPort() const { return m_DestinationPort; }

    private:
        LV2_Evbuf_Iterator* m_DestinationPort;
    };
    class AuxMidiInMessage : public ringbuf::PacketBase
    {
    public:
        AuxMidiInMessage(const void *data, size_t size, const Data::TMidiAuxInPort::TCallback *callback) : ringbuf::PacketBase(size, data), m_Callback(callback) {}
        void Call() const;

    private:
        const Data::TMidiAuxInPort::TCallback *m_Callback;
    };
    class AuxMidiOutMessage : public ringbuf::PacketBase
    {
    public:
        AuxMidiOutMessage(const void *data, size_t size, jack_port_t *port) : ringbuf::PacketBase(size, data), m_Port(port) {}
        jack_port_t* Port() const { return m_Port; }

    private:
        jack_port_t *m_Port;
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
        void Process(jack_nframes_t nframes);
        void SetDataFromMainThread(Data &&data);
        void SendAtomPortEventFromMainThread(lilvutils::TConnection<lilvutils::TAtomPort>* connection, uint32_t frames, uint32_t subframes, LV2_URID type, uint32_t size, const void* body);
        void SendControlValueFromMainThread(lilvutils::TConnection<lilvutils::TControlPort>* connection, float value);
        void DeferredExecuteAfterRoundTrip(std::function<void()> &&function);
        ringbuf::RingBuf& RingBufToRtThread() { return m_RingBufToRtThread; }
        ringbuf::RingBuf& RingBufFromRtThread() { return m_RingBufFromRtThread; }
        void ProcessMessagesInMainThread(); // should be called regularly
        const lilvutils::RealtimeThreadInterface& realtimeThreadInterface() const { return m_RealtimeThreadInterface; }
        void SendMidiFromMainThread(const void *data, size_t size, jack_port_t *port);
        void SendMidiToPluginFromMainThread(const void *data, size_t size, LV2_Evbuf_Iterator* destinationPort);
        
    private:
        void ProcessMessagesInRealtimeThread(jack_nframes_t nframes);
        void SendPendingAsyncFunctionMessages();
        void ResetEvBufs();
        void ProcessOutputPorts();
        void ProcessOutputPortsForInstance(lilvutils::Instance &instance);
        void RunInstances(jack_nframes_t nframes);
        void ProcessOutgoingAudio(jack_nframes_t nframes);
        void ProcessIncomingMidi(jack_nframes_t nframes);
        void RunReverbInstance(jack_nframes_t nframes);
        void AddReverb(jack_nframes_t nframes);
        void ClearOutputMidiBuffers(jack_nframes_t nframes);

    private:
        std::unique_ptr<Data> m_CurrentData;
        const Data* m_DataInRtThread;
        jack_nframes_t m_Bufsize;
        ringbuf::RingBuf m_RingBufToRtThread {130000, 4096};
        ringbuf::RingBuf m_RingBufFromRtThread {1300000, 4096};
        LV2_URID m_UridMidiEvent;
        std::array<AsyncFunctionMessage, 400> m_BufferForAsyncFunctionMessages;
        size_t m_NumStoredAsyncFunctionMessages = 0;
        lilvutils::RealtimeThreadInterface m_RealtimeThreadInterface;
    };
}