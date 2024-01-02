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
        std::vector<uint32_t> audioOutputPortIndices;

        uint32_t numports = lilv_plugin_get_num_ports(m_Plugin);
        lilvutils::Uri uri_connectionOptional(LV2_CORE__connectionOptional);
        lilvutils::Uri uri_InputPort(LV2_CORE__InputPort);
        lilvutils::Uri uri_OutputPort(LV2_CORE__OutputPort);
        lilvutils::Uri uri_notOnGUI(LV2_PORT_PROPS__notOnGUI);
        lilvutils::Uri uri_ControlPort(LV2_CORE__ControlPort);
        lilvutils::Uri uri_control(LV2_CORE__control);
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

            if(!optional)
            {
                if(isaudio && (!isinput))
                {
                    audioOutputPortIndices.push_back(portindex);
                }
                if(ismidiport && (isinput))
                {
                    controlPortIndices.push_back(portindex);
                }
            }
        }
        if(controlPortIndices.size() > 1)
        {
            // multiple control ports:
            // find the port with the http://lv2plug.in/ns/lv2core#control designation:
            const LilvPort* control_input = lilv_plugin_get_port_by_designation(m_Plugin, uri_InputPort.get(), uri_control.get());
            if (control_input) 
            {
                auto index = lilv_port_get_index(m_Plugin, control_input);
                if(std::find(controlPortIndices.begin(), controlPortIndices.end(), index) != controlPortIndices.end())
                {
                    controlPortIndices = {index};
                }
            }
        }
        if(controlPortIndices.size() > 0)
        {
            m_ControlInputIndex = controlPortIndices[0];

        }
        if(audioOutputPortIndices.size() > 0)
        {
            m_AudioOutputIndex[0] = audioOutputPortIndices[0];
            m_AudioOutputIndex[1] = audioOutputPortIndices[audioOutputPortIndices.size() > 1? 1:0];
        }
    }

    Instance::Instance(const Plugin &plugin, double sample_rate) : m_Plugin(plugin)
    {
        auto uri_workerinterface = lilvutils::Uri(LV2_WORKER__interface);
        bool needsWorker = lilv_plugin_has_extension_data(plugin.get(),uri_workerinterface.get());
        
        // if (lilv_plugin_has_extension_data(plugin.get(),uri_workerinterface))
        // {
        //     jalv->worker                = jalv_worker_new(&jalv->work_lock, true);
        //     jalv->features.sched.handle = jalv->worker;
        //     if (jalv->safe_restore) {
        //     jalv->state_worker           = jalv_worker_new(&jalv->work_lock, false);
        //     jalv->features.ssched.handle = jalv->state_worker;
        // }

        m_Features = World::Static().Features();
        m_Log.handle  = &m_Logger;
        m_Log.printf  = &logger::Logger::printf_static;
        m_Log.vprintf = &logger::Logger::vprintf_static;
        m_LogFeature.data = &m_Log;
        m_LogFeature.URI = LV2_LOG__log;
        m_Features.push_back(&m_LogFeature);
        if(needsWorker)
        {
            m_ScheduleWorker = std::make_unique<schedule::Worker>(*this);
            m_Features.push_back(&m_ScheduleWorker->ScheduleFeature());
        }
        m_Features.push_back(nullptr);
        m_Instance = lilv_plugin_instantiate(plugin.get(), sample_rate, m_Features.data());
        if(!m_Instance)
        {
            throw std::runtime_error("could not instantiate plugin");
        }

        uint32_t numports = lilv_plugin_get_num_ports(m_Plugin.get());
        lilvutils::Uri uri_InputPort(LV2_CORE__InputPort);
        lilvutils::Uri uri_OutputPort(LV2_CORE__OutputPort);
        lilvutils::Uri uri_notOnGUI(LV2_PORT_PROPS__notOnGUI);
        lilvutils::Uri uri_ControlPort(LV2_CORE__ControlPort);
        lilvutils::Uri uri_control(LV2_CORE__control);
        lilvutils::Uri uri_AudioPort(LV2_CORE__AudioPort);
        lilvutils::Uri uri_CVPort(LV2_CORE__CVPort);
        lilvutils::Uri uri_AtomPort(LV2_ATOM__AtomPort);
        lilvutils::Uri uri_minimumSize(LV2_RESIZE_PORT__minimumSize);
        lilvutils::Uri uri_midievent(LV2_MIDI__MidiEvent);
        m_Evbufs.resize(numports);
        for(uint32_t portindex = 0; portindex < numports; portindex++)
        {
            auto port = lilv_plugin_get_port_by_index(m_Plugin.get(), portindex);
            bool isatomport = lilv_port_is_a(m_Plugin.get(), port, uri_AtomPort.get());
            if(isatomport)
            {
                bool isinput = lilv_port_is_a(m_Plugin.get(), port, uri_InputPort.get());
                bool isoutput = lilv_port_is_a(m_Plugin.get(), port, uri_OutputPort.get());
                if( (!isinput) && (!isoutput) )
                {
                    throw std::runtime_error("port is neither input nor output");
                }
                int bufsize = 4096;
                std::optional<int> minsize;
                LilvNode* min_size = lilv_port_get(m_Plugin.get(), port, uri_minimumSize.get());
                if (min_size && lilv_node_is_int(min_size))
                {
                    int minsize = lilv_node_as_int(min_size);
                    bufsize = std::max(bufsize, minsize);
                }
                m_Evbufs[portindex] = std::make_unique<Evbuf>(bufsize);
                lilv_instance_connect_port(                    m_Instance, portindex, lv2_evbuf_get_buffer(m_Evbufs[portindex]->get()));
                lv2_evbuf_reset(m_Evbufs[portindex]->get(), isinput);
            }
        }
        if (needsWorker)
        {
            auto worker_iface = (const LV2_Worker_Interface*)lilv_instance_get_extension_data(m_Instance, LV2_WORKER__interface);
            m_ScheduleWorker->Start(worker_iface);
        }
        lilv_instance_activate(m_Instance);
    }
    Instance::~Instance()
    {
        if(m_ScheduleWorker)
        {
            m_ScheduleWorker->Stop();
            m_ScheduleWorker.reset();
        }
        if(m_Instance)
        {
            lilv_instance_deactivate(m_Instance);
            lilv_instance_free(m_Instance);
        }
    }
    LV2_Evbuf* Instance::MidiInEvBuf() const
    {
        if(m_Plugin.ControlInputIndex())
        {
            auto ptr = m_Evbufs.at(m_Plugin.ControlInputIndex().value()).get();
            if(ptr)
            {
                return ptr->get();
            }
        }
        return nullptr;
    }
}

