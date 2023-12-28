#include "lilvjacklink.h"
#include <jack/jack.h>
#include <jack/midiport.h>

namespace lilvjacklink
{
    PortLink::PortLink(std::unique_ptr<jackutils::Port> &&jackport, lilvutils::Instance &instance, uint32_t portindex)
        : m_JackPort(std::move(jackport)), m_Instance(instance), m_PortIndex(portindex)
    {
        auto urid_chunk = lilvutils::World::Static().UriMapLookup(LV2_ATOM__Chunk);
        auto urid_sequence = lilvutils::World::Static().UriMapLookup(LV2_ATOM__Sequence);
        LV2_Evbuf* evbuf = nullptr;
        utils::finally fin1([&](){
            if(evbuf)
            {
                lv2_evbuf_free(evbuf); 
            }
        });
        if(m_JackPort->kind() == jackutils::Port::Kind::Midi)
        {
            uint32_t bufsize = 100000; // bufsizeOrDefault.value_or(100000);
            evbuf = lv2_evbuf_new(bufsize, urid_chunk, urid_sequence);
            if(!evbuf)
            {
                throw std::runtime_error("could not create evbuf");
            }
            lv2_evbuf_reset(evbuf, m_JackPort->direction() == jackutils::Port::Direction::Input);
            lilv_instance_connect_port(m_Instance.get(), m_PortIndex, lv2_evbuf_get_buffer(evbuf));
        }
        m_Evbuf = evbuf;
        evbuf = nullptr;
        m_UridMidiEvent = lilvutils::World::Static().UriMapLookup(LV2_MIDI__MidiEvent);
        {
            jackutils::ProcessBlocker block;
            Global::Static().m_PortLinks.push_back(this);
        }
    }

    PortLink::~PortLink()
    {
        {
            jackutils::ProcessBlocker block;
            auto &links = Global::Static().m_PortLinks;
            auto it = std::find(links.begin(), links.end(), this);
            if(it != links.end())
            {
                links.erase(it);
            }
        }
        lilv_instance_connect_port(m_Instance.get(), m_PortIndex, nullptr);
    }

    int PortLink::process1(jack_nframes_t nframes)
    {
        if(m_JackPort->kind() == jackutils::Port::Kind::Audio)
        {
            auto buf = jack_port_get_buffer(m_JackPort->get(), nframes);
            lilv_instance_connect_port(m_Instance.get(), m_PortIndex, buf);
        }
        else if(m_JackPort->kind() == jackutils::Port::Kind::Midi)
        {
            if(m_JackPort->direction() == jackutils::Port::Direction::Input)
            {
                lv2_evbuf_reset(m_Evbuf, true);
                void* buf = jack_port_get_buffer(m_JackPort->get(), nframes);
                if(buf)
                {
                    LV2_Evbuf_Iterator iter = lv2_evbuf_begin(m_Evbuf);
                    for (uint32_t i = 0; i < jack_midi_get_event_count(buf); ++i) 
                    {
                        jack_midi_event_t ev;
                        jack_midi_event_get(&ev, buf, i);
                        lv2_evbuf_write(&iter, ev.time, 0, m_UridMidiEvent, ev.size, ev.buffer);
                    }
                }
            }
        }
        return 0;
    }
    int PortLink::process2(jack_nframes_t nframes)
    {
        // deliver midi out after the plugin has updated
        if( (m_JackPort->kind() == jackutils::Port::Kind::Midi)
         && (m_JackPort->direction() == jackutils::Port::Direction::Output) )
        {
            void* buf = jack_port_get_buffer(m_JackPort->get(), nframes);
            if(buf)
            {
                jack_midi_clear_buffer(buf);

                for (LV2_Evbuf_Iterator i = lv2_evbuf_begin(m_Evbuf);
                    lv2_evbuf_is_valid(i);
                    i = lv2_evbuf_next(i))
                {
                    // Get event from LV2 buffer
                    uint32_t frames    = 0;
                    uint32_t subframes = 0;
                    LV2_URID type      = 0;
                    uint32_t size      = 0;
                    jack_midi_data_t*    body      = nullptr;
                    lv2_evbuf_get(i, &frames, &subframes, &type, &size, (void**)&body);

                    if (buf && type == m_UridMidiEvent) 
                    {
                        // Write MIDI event to Jack output
                        jack_midi_event_write(buf, frames, body, size);
                    }
                }
            }
        }
        return 0;
    }
    int Global::process(jack_nframes_t nframes)
    {
        for(const auto &link: m_PortLinks)
        {
            link->process1(nframes);
        }
        for(const auto &link: m_InstanceLinks)
        {
            link->process(nframes);
        }
        for(const auto &link: m_PortLinks)
        {
            link->process2(nframes);
        }
        return 0;
    }

    LinkedPluginInstance::LinkedPluginInstance(lilvutils::Instance &instance, const std::string &instancename) : m_Instance(instance)
    {
        auto lilvplugin = m_Instance.plugin().get();
        uint32_t numports = lilv_plugin_get_num_ports(lilvplugin);
        lilvutils::Uri uri_connectionOptional(LV2_CORE__connectionOptional);
        lilvutils::Uri uri_InputPort(LV2_CORE__InputPort);
        lilvutils::Uri uri_OutputPort(LV2_CORE__OutputPort);
        lilvutils::Uri uri_notOnGUI(LV2_PORT_PROPS__notOnGUI);
        lilvutils::Uri uri_ControlPort(LV2_CORE__ControlPort);
        lilvutils::Uri uri_AudioPort(LV2_CORE__AudioPort);
        lilvutils::Uri uri_CVPort(LV2_CORE__CVPort);
        lilvutils::Uri uri_AtomPort(LV2_ATOM__AtomPort);
        lilvutils::Uri uri_minimumSize(LV2_RESIZE_PORT__minimumSize);
        lilvutils::Uri uri_midievent(LV2_MIDI__MidiEvent);
        for(uint32_t portindex = 0; portindex < numports; portindex++)
        {
            auto port = lilv_plugin_get_port_by_index(lilvplugin, portindex);
            auto optional = lilv_port_has_property(lilvplugin, port, uri_connectionOptional.get());
            auto notongui = lilv_port_has_property(lilvplugin, port, uri_notOnGUI.get());
            bool isinput = lilv_port_is_a(lilvplugin, port, uri_InputPort.get());
            bool isoutput = lilv_port_is_a(lilvplugin, port, uri_OutputPort.get());
            bool isaudio = lilv_port_is_a(lilvplugin, port, uri_AudioPort.get());
            bool iscontrolport = lilv_port_is_a(lilvplugin, port, uri_ControlPort.get());
            bool iscvport = lilv_port_is_a(lilvplugin, port, uri_CVPort.get());
            bool isatomport = lilv_port_is_a(lilvplugin, port, uri_AtomPort.get());
            bool ismidiport = isatomport && lilv_port_supports_event(lilvplugin, port, uri_midievent.get());
            if( (!isinput) && (!isoutput) )
            {
                throw std::runtime_error("port is neither input nor output");
            }
            const LilvNode* portsymbol = lilv_port_get_symbol(lilvplugin, port);

            std::optional<uint32_t> bufsizeOrDefault;
            {
                auto min_size = lilv_port_get(lilvplugin, port, uri_minimumSize.get());
                if (min_size && lilv_node_is_int(min_size)) {
                    bufsizeOrDefault = lilv_node_as_int(min_size);
                }
                lilv_node_free(min_size);
            }
            if( (!optional) && (!notongui) )
            {
                if(!portsymbol){
                    throw std::runtime_error("port has no symbol");
                }
                std::string portsymbolstr = lilv_node_as_string(portsymbol);
                auto jackportname = instancename+"_"+portsymbolstr;
                if(isaudio || ismidiport)
                {
                    auto port = std::make_unique<jackutils::Port>(std::move(jackportname), isaudio? jackutils::Port::Kind::Audio : jackutils::Port::Kind::Midi, isinput? jackutils::Port::Direction::Input : jackutils::Port::Direction::Output);
                    m_PortLinks.push_back(std::make_unique<PortLink>(std::move(port), m_Instance, portindex));
                }
            }
        }
        lilv_instance_activate(m_Instance.get());
        {
            jackutils::ProcessBlocker block;
            Global::Static().m_InstanceLinks.push_back(this);
        }
    }
    LinkedPluginInstance::~LinkedPluginInstance()
    {
        {
            jackutils::ProcessBlocker block;
            auto &links = Global::Static().m_InstanceLinks;
            auto it = std::find(links.begin(), links.end(), this);
            if(it != links.end())
            {
                links.erase(it);
            }
        }
    }
    int LinkedPluginInstance::process(jack_nframes_t nframes)
    {
        lilv_instance_run(m_Instance.get(), nframes);
        return 0;
    }
}