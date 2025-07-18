#include "realtimethread.h"

#pragma clang optimize on

namespace realtimethread
{
    void Processor::Process(jack_nframes_t nframes)
    {
        if(nframes > m_Bufsize)
        {
            throw std::runtime_error("nframes > bufsize");
        }
        if( (nframes & 7) != 0)
        {
            throw std::runtime_error("nframes not a multiple of 8");
        }
        ClearOutputMidiBuffers(nframes);
        ProcessMessagesInRealtimeThread(nframes);
        ProcessIncomingMidi(nframes);
        ProcessIncomingAudio(nframes);
        RunInstances(nframes);
        ProcessOutgoingAudio(nframes);
        RunReverbInstance(nframes);
        AddReverb(nframes);
        ProcessOutputLevel(nframes);
        ProcessOutputPorts();
        ResetEvBufs();
        SendPendingAsyncFunctionMessages();
    }

    void Processor::SetDataFromMainThread(Data &&data)
    {
        auto olddata = std::move(m_CurrentData);
        m_CurrentData = std::make_unique<Data>(std::move(data));
        RingBufToRtThread().Write(SetDataMessage(m_CurrentData.get()));
        auto ptr = olddata.release();
        DeferredExecuteAfterRoundTrip([ptr](){
            delete ptr;
        });
    }
    void Processor::SendAtomPortEventFromMainThread(lilvutils::TConnection<lilvutils::TAtomPort>* connection, uint32_t frames, uint32_t subframes, LV2_URID type, uint32_t size, const void* body)
    {
        RingBufToRtThread().Write(realtimethread::AtomPortEventMessage(connection, frames, subframes, type, size, body));
    }
    void Processor::SendControlValueFromMainThread(lilvutils::TConnection<lilvutils::TControlPort>* connection, float value)
    {
        Processor::RingBufToRtThread().Write(realtimethread::ControlPortChangedMessage(connection, value));
    }
    void Processor::DeferredExecuteAfterRoundTrip(std::function<void()> &&function)
    {
        RingBufToRtThread().Write(realtimethread::AsyncFunctionMessage(std::move(function)));
    }
    void Processor::ProcessMessagesInMainThread()
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
            else if(auto auxMidiInMessage = dynamic_cast<const AuxMidiInMessage*>(message))
            {
                auxMidiInMessage->Call();
            }
            else if(auto outputLevelUpdateMessage = dynamic_cast<const OutputLevelUpdateMessage*>(message))
            {
                UpdateOutputLevelDbInMainThread(outputLevelUpdateMessage->AmplitudeSquared());
            }
        }
    }    
    void Processor::UpdateOutputLevelDbInMainThread(float ampsquared)
    {
        m_OutputLevelDb = 10.0f * std::log10(ampsquared);
        m_OnOutputLevelChange.Notify();
        auto now = std::chrono::steady_clock::now();
        m_LevelMeterHistory.emplace_back(now, m_OutputLevelDb);
        if(m_OutputLevelDb > m_OutputPeakLevelDb)
        {
            m_OutputPeakLevelDb = m_OutputLevelDb;
        }
        constexpr auto peakHoldTime = std::chrono::milliseconds(1000);
        bool recalculate = false;
        while(!m_LevelMeterHistory.empty() && now - m_LevelMeterHistory.front().first > peakHoldTime)
        {
            if(m_LevelMeterHistory.front().second >= m_OutputPeakLevelDb)
            {
                recalculate = true;
            }
            m_LevelMeterHistory.pop_front();
        }
        if(recalculate)
        {
            float newpeak = - std::numeric_limits<float>::infinity();
            for(const auto &[t, level]: m_LevelMeterHistory)
            {
                newpeak = std::max(newpeak, level);
            }
            m_OutputPeakLevelDb = newpeak;
        }
    }

    void Processor::ProcessMessagesInRealtimeThread(jack_nframes_t nframes)
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
                auto connection = cpchangedmessage->Connection();
                *connection->Buffer() = cpchangedmessage->NewValue();
                connection->OrigValue() = cpchangedmessage->NewValue();
            }
            else if(auto atomPortEventMessage = dynamic_cast<const AtomPortEventMessage*>(message))
            {
                auto &bufferIterator = atomPortEventMessage->Connection()->BufferIterator();
                lv2_evbuf_write(&bufferIterator, atomPortEventMessage->Frames(), atomPortEventMessage->SubFrames(), atomPortEventMessage->Type(), atomPortEventMessage->AdditionalDataSize(), atomPortEventMessage->AdditionalDataBuf());
            }                
            else if(auto midioutmessage = dynamic_cast<const realtimethread::AuxMidiOutMessage*>(message); midioutmessage)
            {
                auto buf = jack_port_get_buffer(midioutmessage->Port(), nframes);
                jack_midi_event_write(buf, 0, (const jack_midi_data_t*) midioutmessage->AdditionalDataBuf(), midioutmessage->AdditionalDataSize());

            }
            else if(auto midiMessageToPlugin = dynamic_cast<const realtimethread::TMidiMessageToPlugin*>(message); midiMessageToPlugin)
            {
                lv2_evbuf_write(midiMessageToPlugin->DestinationPort(), 0, 0, m_UridMidiEvent, midiMessageToPlugin->AdditionalDataSize(), midiMessageToPlugin->AdditionalDataBuf());
            }
        }
    }
    void Processor::SendPendingAsyncFunctionMessages()
    {
        for(size_t i=0; i < m_NumStoredAsyncFunctionMessages; ++i)
        {
            // post back to main thread
            RingBufFromRtThread().Write(m_BufferForAsyncFunctionMessages[i]);
        }
        m_NumStoredAsyncFunctionMessages = 0;
    }
    void Processor::ResetEvBufs()
    {
        if(!m_DataInRtThread) return;
        const auto &data = *m_DataInRtThread;
        for(const auto &plugin: data.Plugins())
        {
            auto &instance = plugin.PluginInstance();
            for(auto &connection: instance.Connections())
            {
                if(auto atomportconnection = dynamic_cast<lilvutils::TConnection<lilvutils::TAtomPort>*>(connection.get()); atomportconnection)
                {
                    atomportconnection->ResetEvBuf();
                }
            }
        }
    }
    void Processor::ClearOutputMidiBuffers(jack_nframes_t nframes)
    {
        if(!m_DataInRtThread) return;
        const auto &data = *m_DataInRtThread;
        for(const auto &outport: data.MidiAuxOutPorts())
        {
            auto buf = jack_port_get_buffer(outport.Port(), nframes);
            jack_midi_clear_buffer(buf);            
        }
    }

    void Processor::ProcessOutputPorts()
    {
        if(!m_DataInRtThread) return;
        const auto &data = *m_DataInRtThread;
        for(const auto &plugin: data.Plugins())
        {
            auto &instance = plugin.PluginInstance();
            ProcessOutputPortsForInstance(instance);
        }
        if(data.ReverbInstance())
        {
            ProcessOutputPortsForInstance(*data.ReverbInstance());
        }
    }

    void Processor::ProcessOutputPortsForInstance(lilvutils::Instance &instance)
    {
        for(auto &connection: instance.Connections())
        {
            if(auto controlportconnection = dynamic_cast<lilvutils::TConnection<lilvutils::TControlPort>*>(connection.get()); controlportconnection)
            {
                const auto &port = controlportconnection->Port();
                //if(port.Direction() == lilvutils::TPortBase::TDirection::Output)
                {
                    if(*controlportconnection->Buffer() != controlportconnection->OrigValue())
                    {
                        controlportconnection->OrigValue() = *controlportconnection->Buffer();
                        RingBufFromRtThread().Write(ControlPortChangedMessage(controlportconnection, *controlportconnection->Buffer()));
                    }
                }
            }
            else if(auto atomportconnection = dynamic_cast<lilvutils::TConnection<lilvutils::TAtomPort>*>(connection.get()); atomportconnection)
            {
                const auto &port = atomportconnection->Port();
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

                        RingBufFromRtThread().Write(AtomPortEventMessage(atomportconnection, frames, subframes, type, size, body), false);
                        atomportconnection->BufferIterator() = lv2_evbuf_next(atomportconnection->BufferIterator());
                    }
                }
            }
        }
    }
    void Processor::RunInstances(jack_nframes_t nframes)
    {
        if(!m_DataInRtThread) return;
        const auto &data = *m_DataInRtThread;
        for(const auto &plugin: data.Plugins())
        {
            plugin.PluginInstance().Run(nframes);
        }
    }
    void Processor::ProcessIncomingAudio(jack_nframes_t nframes)
    {
        if(!m_DataInRtThread) return;
        const auto &data = *m_DataInRtThread;
        auto vocoderbuf = data.VocoderInPort()? (const float*)jack_port_get_buffer(data.VocoderInPort(), nframes) : (const float*) nullptr;
        for(const auto &plugin: data.Plugins())
        {
            if(plugin.HasVocoderInput())
            {
                for(size_t channel: {0,1})
                {
                    auto inputPortindexOrNull = plugin.PluginInstance().plugin().InputAudioPortIndices()[channel];
                    if(inputPortindexOrNull)
                    {
                        float *destbuffer = dynamic_cast<lilvutils::TConnection<lilvutils::TAudioPort>*>(plugin.PluginInstance().Connections().at(*inputPortindexOrNull).get())->Buffer();
                        if(destbuffer)
                        {
                            // for(size_t i = 0; i < nframes; ++i)
                            // {
                            //     destbuffer[i] = i&8? 0.5f : -0.5f;
                            // }
                            if(vocoderbuf)
                            {
                                for(size_t i = 0; i < nframes; i++)
                                {
                                    destbuffer[i] = vocoderbuf[i];
                                }
                            }
                            else
                            {
                                std::fill(destbuffer, destbuffer + nframes, 0.0f);
                            }
                        }
                    }
                }
            }
        }
    }
    void Processor::RunReverbInstance(jack_nframes_t nframes)
    {
        if(!m_DataInRtThread) return;
        const auto &data = *m_DataInRtThread;
        if(data.ReverbInstance())
        {
            data.ReverbInstance()->Run(nframes);
        }
    }
    void Processor::ProcessOutputLevel(jack_nframes_t nframes)
    {
        if(!m_DataInRtThread) return;
        const auto &data = *m_DataInRtThread;
        std::array<float*, 2> mixedAudioPorts = {
            data.OutputAudioPorts()[0]? (float*)jack_port_get_buffer(data.OutputAudioPorts()[0], nframes) : nullptr,
            data.OutputAudioPorts()[1]? (float*)jack_port_get_buffer(data.OutputAudioPorts()[1], nframes) : nullptr
        };
        bool hasoutputaudio = mixedAudioPorts[0] && mixedAudioPorts[1];
        if(hasoutputaudio)
        {
            float y = m_LevelMeterOutputBuf[0];
            float a = data.LevelMeterTimeConstant();
            float b = 1.0f - a;
            size_t index = 0;
            for(size_t i = 0; i < nframes / 8; ++i)
            {
                std::array<float, 8> sumsqaured;
                for(size_t j = 0; j < 8; ++j)
                {
                    float sum = mixedAudioPorts[0][index] + mixedAudioPorts[1][index];
                    sumsqaured[j] = sum * sum;
                    index++;
                }
                for(size_t j = 0; j < 8; ++j)
                {
                    float newy = b * sumsqaured[j] + a * y;
                    y = newy;
                }
            }
            m_LevelMeterOutputBuf[0] = y;
        }
        else
        {
            m_LevelMeterOutputBuf[0] = 0.0f;
        }
        m_LevelMeterOutputSampleCounter += nframes;
        int update_hz = 30;
        if(m_LevelMeterOutputSampleCounter >= 48000/update_hz)
        {
            m_LevelMeterOutputSampleCounter = 0;
            RingBufFromRtThread().Write(OutputLevelUpdateMessage(m_LevelMeterOutputBuf[0]));            
        }
    }

    void Processor::AddReverb(jack_nframes_t nframes)
    {
        if(!m_DataInRtThread) return;
        const auto &data = *m_DataInRtThread;
        std::array<float*, 2> mixedAudioPorts = {
            data.OutputAudioPorts()[0]? (float*)jack_port_get_buffer(data.OutputAudioPorts()[0], nframes) : nullptr,
            data.OutputAudioPorts()[1]? (float*)jack_port_get_buffer(data.OutputAudioPorts()[1], nframes) : nullptr
        };
        bool hasoutputaudio = mixedAudioPorts[0] && mixedAudioPorts[1];
        if(hasoutputaudio && data.ReverbInstance() && data.ReverbLevel() != 0.0f)
        {
            // copy mixed audio to reverb input:
            for(size_t channel: {0,1})
            {
                auto portindexOrNull = data.ReverbInstance()->plugin().OutputAudioPortIndices()[channel];
                if(portindexOrNull)
                {
                    auto portindex = *portindexOrNull;
                    auto outputAudioPort = dynamic_cast<lilvutils::TConnection<lilvutils::TAudioPort>*>(data.ReverbInstance()->Connections().at(portindex).get());
                    auto reverbOutputBuffer = outputAudioPort->Buffer();
                    auto mixedAudioBuffer = mixedAudioPorts[channel];
                    for(size_t i = 0; i < nframes; ++i)
                    {
                        mixedAudioBuffer[i] += data.ReverbLevel() * reverbOutputBuffer[i];
                    }
                }
            }
        }
    }
    void Processor::ProcessOutgoingAudio(jack_nframes_t nframes)
    {
        if(!m_DataInRtThread) return;
        const auto &data = *m_DataInRtThread;
        std::array<float*, 2> mixedAudioPorts = {
            data.OutputAudioPorts()[0]? (float*)jack_port_get_buffer(data.OutputAudioPorts()[0], nframes) : nullptr,
            data.OutputAudioPorts()[1]? (float*)jack_port_get_buffer(data.OutputAudioPorts()[1], nframes) : nullptr
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
        if(hasoutputaudio && data.ReverbInstance())
        {
            // copy mixed audio to reverb input:
            for(size_t channel: {0,1})
            {
                auto portindexOrNull = data.ReverbInstance()->plugin().InputAudioPortIndices()[channel];
                if(portindexOrNull)
                {
                    auto portindex = *portindexOrNull;
                    auto inputAudioPort = dynamic_cast<lilvutils::TConnection<lilvutils::TAudioPort>*>(data.ReverbInstance()->Connections().at(portindex).get());
                    auto reverbInputBuffer = inputAudioPort->Buffer();
                    auto mixedAudioBuffer = mixedAudioPorts[channel];
                    std::copy(mixedAudioBuffer, mixedAudioBuffer + nframes, reverbInputBuffer);
                }
            }
        }
    }
    
    void Processor::ProcessIncomingMidi(jack_nframes_t nframes)
    {
        if(!m_DataInRtThread) return;
        const auto &data = *m_DataInRtThread;
        for(const auto &midiport: data.MidiPorts())
        {
            auto pluginindex = midiport.PluginIndex();
            auto jackport = &midiport.Port();
            auto buf = jack_port_get_buffer(jackport, nframes);
            auto evtcount = jack_midi_get_event_count(buf);
            for (uint32_t i = 0; i < evtcount; ++i) 
            {
                jack_midi_event_t ev;
                jack_midi_event_get(&ev, buf, i);
                if(midi::SimpleEvent::IsSupported(ev.buffer, ev.size))
                {
                    auto event = midi::SimpleEvent(ev.buffer, ev.size);
                    if(pluginindex)
                    {
                        auto &plugin = data.Plugins()[*pluginindex];
                        int targetChannel = 0;
                        if(plugin.DoOverrideChannel())
                        {
                            targetChannel = midiport.OverrideChannel();
                        }
                        auto modifiedevent = event.ChangeChannel(targetChannel).Transpose(plugin.Transpose());
                        if(modifiedevent && plugin.MidiInBuf())
                        {
                            lv2_evbuf_write(plugin.MidiInBuf(), ev.time, 0, m_UridMidiEvent, modifiedevent->Size(), modifiedevent->Buffer());
                        }
                    }
                }
            }
        }
        for(const auto &auxinport: data.MidiAuxInPorts())
        {
            auto buf = jack_port_get_buffer(auxinport.Port(), nframes);
            auto evtcount = jack_midi_get_event_count(buf);
            for (uint32_t i = 0; i < evtcount; ++i) 
            {
                jack_midi_event_t ev;
                jack_midi_event_get(&ev, buf, i);
                RingBufFromRtThread().Write(AuxMidiInMessage((const void *)ev.buffer, ev.size, auxinport.Callback()), false);
            }
        }
    }
    void Processor::SendMidiFromMainThread(const void *data, size_t size, jack_port_t *port)
    {
        RingBufToRtThread().Write(AuxMidiOutMessage(data, size, port));
    }
    void Processor::SendMidiToPluginFromMainThread(const void *data, size_t size, LV2_Evbuf_Iterator* destinationPort) 
    {
        RingBufToRtThread().Write(TMidiMessageToPlugin(data, size, destinationPort));
    }
    void AuxMidiInMessage::Call() const
    {
        //  called in main thread
        if(m_Callback)
        {
            if(midi::TMidiOrSysexEvent::IsSupported(this->AdditionalDataBuf(), AdditionalDataSize()))
            {
                midi::TMidiOrSysexEvent event(this->AdditionalDataBuf(), AdditionalDataSize());
                (*m_Callback)(event);
            }
        }
    }


} // namespace realtimethread
