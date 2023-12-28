#pragma once
#include <stdexcept>
#include <memory>	
#include <optional>
#include "lilvutils.h"
#include "jackutils.h"
#include "lv2/port-props/port-props.h"
#include "lv2/atom/atom.h"
#include "lv2/resize-port/resize-port.h"

namespace lilvjacklink
{
    class Link
    {
    public:
        Link(const Link&) = delete;
        Link& operator=(const Link&) = delete;
        Link(Link&&) = delete;
        Link& operator=(Link&&) = delete;
        Link();
        ~Link();
        int process(jack_nframes_t nframes);
    };
    class Global
    {
        friend Link;
    public:
        Global(const Global&) = delete;
        Global& operator=(const Global&) = delete;
        Global(Global&&) = delete;
        Global& operator=(Global&&) = delete;
        Global()
        {
            if(staticptr())
            {
                throw std::runtime_error("Can only instantiate a single lilvjacklink::Global");
            }
            staticptr() = this;
        }
        ~Global()
        {
            staticptr() = nullptr;
        }
        int process(jack_nframes_t nframes);
        static Global& Static()
        {
            if(!staticptr())
            {
                throw std::runtime_error("Global not instantiated");
            }
            return *staticptr();
        }
        static Global* StaticPtr()
        {
            return staticptr();
        }
        
    private:
        static Global*& staticptr()
        {
            static Global *s_Global = nullptr;
            return s_Global;
        }

    private:
        std::vector<std::unique_ptr<Link>> m_Links;
    };

    class LinkedPluginInstance
    {
    public:
        LinkedPluginInstance(LinkedPluginInstance&&) = delete;
        LinkedPluginInstance& operator=(LinkedPluginInstance&&) = delete;
        LinkedPluginInstance(const LinkedPluginInstance&) = delete;
        LinkedPluginInstance& operator=(const LinkedPluginInstance&) = delete;
        LinkedPluginInstance(const lilvutils::Instance &instance) : m_Instance(instance)
        {
            auto lilvplugin = instance.plugin().get();
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
            for(uint32_t portindex = 0; portindex < numports; portindex++)
            {
                auto port = lilv_plugin_get_port_by_index(lilvplugin, portindex);
                auto optional = lilv_port_has_property(lilvplugin, port, uri_connectionOptional.get());
                auto notongui = lilv_port_has_property(lilvplugin, port, uri_notOnGUI.get());
                bool isinput = lilv_port_is_a(lilvplugin, port, uri_InputPort.get());
                bool isoutput = lilv_port_is_a(lilvplugin, port, uri_OutputPort.get());
                bool isoutput = lilv_port_is_a(lilvplugin, port, uri_OutputPort.get());
                bool iscontrolport = lilv_port_is_a(lilvplugin, port, uri_ControlPort.get());
                bool iscvport = lilv_port_is_a(lilvplugin, port, uri_CVPort.get());
                bool isatomport = lilv_port_is_a(lilvplugin, port, uri_AtomPort.get());
                if( (!isinput) && (!isoutput) )
                {
                    throw std::runtime_error("port is neither input nor output");
                }
                std::optional<uint32_t> buf_size;
                {
                    auto min_size = lilv_port_get(lilvplugin, port, uri_minimumSize.get());
                    if (min_size && lilv_node_is_int(min_size)) {
                        buf_size = lilv_node_as_int(min_size);
                    }
                    lilv_node_free(min_size);
                }




            }

        }
    private:
        const lilvutils::Instance &m_Instance;
        std::vector<std::unique_ptr<Link>> m_Links;
    };



}