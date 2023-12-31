#include "lilvutils.h"
#include "lv2/port-props/port-props.h"
#include "lv2/atom/atom.h"
#include "lv2/resize-port/resize-port.h"
#include "lv2_evbuf.h"
#include "lv2/midi/midi.h"

namespace lilvutils
{
    Plugin::Plugin(const Uri &uri) : m_Uri(uri.str())
    {
        auto lilvworld = World::Static().get();
        auto plugins = World::Static().Plugins();
        auto urinode = uri.get();
        m_Plugin = lilv_plugins_get_by_uri(plugins, urinode); // return value must not be freed
        if(!m_Plugin)
        {
            throw std::runtime_error("could not load plugin");
        }
        std::vector<uint32_t> controlPortIndices;
        std::vector<uint32_t> audioPortIndices;

        uint32_t numports = lilv_plugin_get_num_ports(m_Plugin);
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
            auto port = lilv_plugin_get_port_by_index(m_Plugin, portindex);
            auto optional = lilv_port_has_property(m_Plugin, port, uri_connectionOptional.get());
            auto notongui = lilv_port_has_property(m_Plugin, port, uri_notOnGUI.get());
            bool isinput = lilv_port_is_a(m_Plugin, port, uri_InputPort.get());
            bool isoutput = lilv_port_is_a(m_Plugin, port, uri_OutputPort.get());
            bool isaudio = lilv_port_is_a(m_Plugin, port, uri_AudioPort.get());
            bool iscontrolport = lilv_port_is_a(m_Plugin, port, uri_ControlPort.get());
            bool iscvport = lilv_port_is_a(m_Plugin, port, uri_CVPort.get());
            bool isatomport = lilv_port_is_a(m_Plugin, port, uri_AtomPort.get());
            bool ismidiport = isatomport && lilv_port_supports_event(m_Plugin, port, uri_midievent.get());
            if( (!isinput) && (!isoutput) )
            {
                throw std::runtime_error("port is neither input nor output");
            }
            const LilvNode* portsymbol = lilv_port_get_symbol(m_Plugin, port);

            std::optional<uint32_t> bufsizeOrDefault;
            {
                auto min_size = lilv_port_get(m_Plugin, port, uri_minimumSize.get());
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

        const LilvPort* control_input = lilv_plugin_get_port_by_designation(m_Plugin, uri_InputPort.get(), uri_control.get());
        if (control_input) {
            auto index = lilv_port_get_index(m_Plugin, control_input);
            auto port = lilv_plugin_get_port_by_index(m_Plugin, index);
            bool isatomport = lilv_port_is_a(m_Plugin, port, uri_AtomPort.get());
            if(isatomport)
            {
                m_ControlInputIndex = index;
            }
        }

    }
}

