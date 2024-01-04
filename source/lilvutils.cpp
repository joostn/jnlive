#include "lilvutils.h"
#include "lv2/port-props/port-props.h"
#include "lv2/atom/atom.h"
#include "lv2/resize-port/resize-port.h"
#include "lv2_evbuf.h"
#include "lv2/midi/midi.h"
#include "lv2/port-groups/port-groups.h"

namespace 
{
    std::vector<std::unique_ptr<lilvutils::TPortBase>> GetPluginPorts(const LilvPlugin *plugin, bool &canInstantiate)
    {
        canInstantiate = true;
        std::vector<std::unique_ptr<lilvutils::TPortBase>> result;
        uint32_t numports = lilv_plugin_get_num_ports(plugin);
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
        lilvutils::Uri uri_portGroupsLeft(LV2_PORT_GROUPS__left);
        lilvutils::Uri uri_portGroupsRight(LV2_PORT_GROUPS__right);

        auto leftoutputport = lilv_plugin_get_port_by_designation(plugin, uri_OutputPort.get(), uri_portGroupsLeft.get());
        auto rightoutputport = lilv_plugin_get_port_by_designation(plugin, uri_OutputPort.get(), uri_portGroupsRight.get());
        auto leftinputport = lilv_plugin_get_port_by_designation(plugin, uri_InputPort.get(), uri_portGroupsLeft.get());
        auto rightinputport = lilv_plugin_get_port_by_designation(plugin, uri_InputPort.get(), uri_portGroupsRight.get());
        auto controloutputport = lilv_plugin_get_port_by_designation(plugin, uri_OutputPort.get(), uri_control.get());
        auto controlinputport = lilv_plugin_get_port_by_designation(plugin, uri_InputPort.get(), uri_control.get());

        for(uint32_t portindex = 0; portindex < numports; portindex++)
        {
            auto port = lilv_plugin_get_port_by_index(plugin, portindex);
            auto optional = lilv_port_has_property(plugin, port, uri_connectionOptional.get());
            auto notongui = lilv_port_has_property(plugin, port, uri_notOnGUI.get());
            bool isinput = lilv_port_is_a(plugin, port, uri_InputPort.get());
            bool isoutput = lilv_port_is_a(plugin, port, uri_OutputPort.get());
            bool isaudio = lilv_port_is_a(plugin, port, uri_AudioPort.get());
            bool iscontrolport = lilv_port_is_a(plugin, port, uri_ControlPort.get());
            bool iscvport = lilv_port_is_a(plugin, port, uri_CVPort.get());
            bool isatomport = lilv_port_is_a(plugin, port, uri_AtomPort.get());

            if(isinput != (!isoutput))
            {
                throw std::runtime_error("port is neither input nor output");
            }
            auto direction = isinput? lilvutils::TPortBase::TDirection::Input : lilvutils::TPortBase::TDirection::Output;
            std::string symbol;
            auto port_sym = lilv_port_get_symbol(plugin, port);
            // Returned value is owned by port and must not be freed
            if(port_sym)
            {
                auto str = lilv_node_as_string(port_sym);
                if(str)
                {
                    symbol = str;
                }
            }
            std::string name;
            auto port_name = lilv_port_get_name(plugin, port);
            if(port_name)
            {
                utils::finally fin1([&](){
                    lilv_node_free(port_name);
                });
                auto str = lilv_node_as_string(port_name);
                if(str)
                {
                    name = str;
                }
            }
            std::unique_ptr<lilvutils::TPortBase> portbase;
            if(isaudio)
            {
                std::optional<lilvutils::TAudioPort::TDesignation> designation;
                if( (port == leftoutputport) || (port == leftinputport) )
                {
                    designation = lilvutils::TAudioPort::TDesignation::StereoLeft;
                }
                else if( (port == rightoutputport) || (port == rightinputport) )
                {
                    designation = lilvutils::TAudioPort::TDesignation::StereoRight;
                }
                portbase = std::make_unique<lilvutils::TAudioPort>(direction, std::move(name), std::move(symbol), designation, optional);
            }
            else if(isatomport)
            {
                std::optional<int> minbufsize;
                {
                    LilvNode* min_size = lilv_port_get(plugin, port, uri_minimumSize.get());
                    if (min_size && lilv_node_is_int(min_size))
                    {
                        minbufsize = lilv_node_as_int(min_size);
                    }
                }
                std::optional<lilvutils::TAtomPort::TDesignation> designation;
                if( (port == controloutputport) || (port == controlinputport) )
                {
                    designation = lilvutils::TAtomPort::TDesignation::Control;
                }
                bool supportsmidi = lilv_port_supports_event(plugin, port, uri_midievent.get());
                portbase = std::make_unique<lilvutils::TAtomPort>(direction, std::move(name), std::move(symbol), designation, supportsmidi, minbufsize, optional);
            }
            else if(iscontrolport)
            {
                bool hasrange = false;
                float minv = 0.0f;
                float maxv = 1.0f;
                float defv = 0.0f;
                LilvNode *minvalue = nullptr;
                LilvNode *maxvalue = nullptr;
                LilvNode *defaultvalue = nullptr;
                lilv_port_get_range(plugin, port, &defaultvalue, &minvalue, &maxvalue);
                utils::finally fin1([&](){
                    if(minvalue) lilv_node_free(minvalue);
                    if(maxvalue) lilv_node_free(maxvalue);
                    if(defaultvalue) lilv_node_free(defaultvalue);
                });
                if( (lilv_node_is_float(minvalue) || lilv_node_is_int(minvalue))
                    && (lilv_node_is_float(maxvalue) || lilv_node_is_int(maxvalue))
                    && (lilv_node_is_float(defaultvalue) || lilv_node_is_int(defaultvalue))
                )
                {
                    minv = lilv_node_as_float(minvalue);
                    maxv = lilv_node_as_float(maxvalue);
                    defv = lilv_node_as_float(defaultvalue);
                    hasrange = true;
                }
                else
                {
                    hasrange = false;
                }
                if( (!hasrange) && (isinput) )
                {
                    // input control ports must have a range. For outputs we don't need it
                    canInstantiate = false;
                }
                else
                {
                    portbase = std::make_unique<lilvutils::TControlPort>(direction, std::move(name), std::move(symbol), minv, maxv, defv, optional);
                }
            }
            else if(iscvport)
            {
                portbase = std::make_unique<lilvutils::TCvPort>(direction, std::move(name), std::move(symbol), optional);
            }
            if( (!portbase) && (!optional) )
            {
                canInstantiate = false;
            }
            result.push_back(std::move(portbase));
        }
        return result;
    }
}
namespace lilvutils
{
    World::World(uint32_t sample_rate, uint32_t maxBlockSize) : m_OptionSampleRate((float)sample_rate), m_OptionMaxBlockLength(maxBlockSize)
    {
        if(staticptr())
        {
            throw std::runtime_error("Can only instantiate a single lilv_world");
        }
        auto world = lilv_world_new();
        if(!world)
        {
            throw std::runtime_error("lilv_world_new failed");
        }
        utils::finally fin1([&](){
            if(world)
            {
                lilv_world_free(world); 
            }
        });
        lilv_world_load_all(world);
        auto plugins = lilv_world_get_all_plugins(world); // result must not be freed
        if(!plugins)
        {
            throw std::runtime_error("Failed to get LV2 plugins.");
        }
/*            LILV_FOREACH(plugins, i, plugins) {
            auto plugin = lilv_plugins_get(plugins, i);
            auto uri = lilv_node_as_uri(lilv_plugin_get_uri(plugin));
            std::cout << "Plugin: " << uri << std::endl;
        }
*/
        m_World = world;
        m_Plugins = plugins;
        world = nullptr;
        plugins = nullptr;
        staticptr() = this;
        m_UridMap.handle = this;
        m_UridMap.map = [](LV2_URID_Map_Handle handle, const char* uri) -> LV2_URID {
            auto world = (World*)handle;
            return (LV2_URID)world->UriMapLookup(uri);
        };
        m_UridUnmap.handle = this;
        m_UridUnmap.unmap = [](LV2_URID_Unmap_Handle handle, LV2_URID urid) -> const char* {
            auto world = (World*)handle;
            return world->UriMapReverseLookup(urid).c_str();
        };

        m_MapFeature.data = &m_UridMap;
        m_MapFeature.URI = LV2_URID__map;
        m_UnmapFeature.data = &m_UridUnmap;
        m_UnmapFeature.URI = LV2_URID__unmap;
        m_Features.push_back(&m_MapFeature);
        m_Features.push_back(&m_UnmapFeature);

        // floats:
        for(auto p: {
            std::make_tuple(LV2_PARAMETERS__sampleRate, &m_OptionSampleRate),
            std::make_tuple(LV2_UI__updateRate, &m_OptionUiUpdateRate),
            std::make_tuple(LV2_UI__scaleFactor, &m_OptionUiScaleFactor),
        })
        {
            m_Options.push_back(LV2_Options_Option {
                .context = LV2_OPTIONS_INSTANCE,
                .subject = 0,
                .key = UriMapLookup(std::get<0>(p)),
                .size = sizeof(float),
                .type = UriMapLookup(LV2_ATOM__Float),
                .value = std::get<1>(p)
            });
        }

        // ints:
        for(auto p: {
            std::make_tuple(LV2_BUF_SIZE__minBlockLength, &m_OptionMinBlockLength),
            std::make_tuple(LV2_BUF_SIZE__maxBlockLength, &m_OptionMaxBlockLength),
            std::make_tuple(LV2_BUF_SIZE__sequenceSize, &m_OptionSequenceSize),
        })
        {
            m_Options.push_back(LV2_Options_Option {
                .context = LV2_OPTIONS_INSTANCE,
                .subject = 0,
                .key = UriMapLookup(std::get<0>(p)),
                .size = sizeof(int32_t),
                .type = UriMapLookup(LV2_ATOM__Int),
                .value = std::get<1>(p)
            });
        }
        // terminate:
        m_Options.push_back(LV2_Options_Option {
            .context = LV2_OPTIONS_INSTANCE,
            .subject = 0,
            .key = 0,
            .size = 0,
            .type = 0,
            .value = nullptr
        });
        m_OptionsFeature.data = m_Options.data();
        m_OptionsFeature.URI = LV2_OPTIONS__options;
        m_Features.push_back(&m_OptionsFeature);
    }
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
        {
            auto namenode = lilv_plugin_get_name(m_Plugin);
            if(namenode)
            {
                utils::finally fin1([&](){
                    lilv_node_free(namenode);
                });
                auto str = lilv_node_as_string(namenode);
                if(str)
                {
                    m_Name = str;
                }
            }
        }
        bool canInstantiate = false;
        m_Ports = GetPluginPorts(m_Plugin, canInstantiate);
        m_CanInstantiate = canInstantiate;
        for(size_t portindex = 0; portindex < m_Ports.size(); portindex++)
        {
            const auto& port = m_Ports[portindex];
            if(auto audioport = dynamic_cast<const TAudioPort*>(port.get()); audioport)
            {
                if(audioport->Direction() == TAudioPort::TDirection::Output)
                {
                    if(audioport->Designation() == TAudioPort::TDesignation::StereoLeft)
                    {
                        m_AudioOutputIndex[0] = portindex;
                        if(!m_AudioOutputIndex[1])
                        {
                            m_AudioOutputIndex[1] = portindex;
                        }
                    }
                    else if(audioport->Designation() == TAudioPort::TDesignation::StereoRight)
                    {
                        m_AudioOutputIndex[1] = portindex;
                        if(!m_AudioOutputIndex[0])
                        {
                            m_AudioOutputIndex[0] = portindex;
                        }
                    }
                    else
                    {
                        if(!m_AudioOutputIndex[0])
                        {
                            m_AudioOutputIndex[0] = portindex;
                        }
                        if(!m_AudioOutputIndex[1])
                        {
                            m_AudioOutputIndex[1] = portindex;
                        }
                    }
                }
            }
            if(auto atomport = dynamic_cast<const TAtomPort*>(port.get()); atomport)
            {
                if(atomport->Direction() == TAtomPort::TDirection::Input)
                {
                    if(atomport->SupportsMidi())
                    {
                        if(atomport->Designation() == TAtomPort::TDesignation::Control)
                        {
                            m_MidiInputIndex = portindex;
                        }
                        else
                        {
                            if(!m_MidiInputIndex)
                            {
                                m_MidiInputIndex = portindex;
                            }
                        }
                    }
                }
            }
        }
    }

    Instance::Instance(const Plugin &plugin, double sample_rate) : m_Plugin(plugin), m_Logger(*this)
    {
        if(!plugin.CanInstantiate())
        {
            throw std::runtime_error("plugin "+plugin.Name()+" hasv nsupported ports and cannot be instantiated");
        }
        auto uri_workerinterface = lilvutils::Uri(LV2_WORKER__interface);
        bool needsWorker = lilv_plugin_has_extension_data(plugin.get(),uri_workerinterface.get());

        m_Features = World::Static().Features();
        m_Features.push_back(m_Logger.Feature());
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
        auto audiobufsize = World::Static().MaxBlockLength();
        for(size_t portindex = 0; portindex < m_Plugin.Ports().size(); portindex++)
        {
            const auto *port = m_Plugin.Ports()[portindex].get();
            std::unique_ptr<TConnectionBase> connection;
            if(port)
            {
                if(dynamic_cast<const TAudioPort*>(port))
                {
                    auto audioconnection = std::make_unique<TConnection<TAudioPort>>(audiobufsize);
                    lilv_instance_connect_port(m_Instance, (uint32_t)portindex, audioconnection->Buffer());
                    connection = std::move(audioconnection);
                }
                else if(dynamic_cast<const TCvPort*>(port))
                {
                    auto audioconnection = std::make_unique<TConnection<TCvPort>>(audiobufsize);
                    lilv_instance_connect_port(m_Instance, (uint32_t)portindex, audioconnection->Buffer());
                    connection = std::move(audioconnection);
                }
                else if(auto atomport = dynamic_cast<const TAtomPort*>(port); atomport)
                {
                    m_PortIndicesOfAtomPorts.push_back(portindex);
                    auto bufsize = atomport->MinBufferSize().value_or(0);
                    bufsize = std::max(bufsize, 65536);
                    auto atomconnection = std::make_unique<TConnection<TAtomPort>>((uint32_t)bufsize);
                    lv2_evbuf_reset(atomconnection->Buffer(), atomport->Direction() == TAtomPort::TDirection::Input);
                    auto actualbuf = lv2_evbuf_get_buffer(atomconnection->Buffer());
                    lilv_instance_connect_port(m_Instance, (uint32_t)portindex, actualbuf);
                    connection = std::move(atomconnection);
                }
                else if(auto controlport = dynamic_cast<const TControlPort*>(port); controlport)
                {
                    auto controlconnection = std::make_unique<TConnection<TControlPort>>(controlport->DefaultValue());
                    lilv_instance_connect_port(m_Instance, (uint32_t)portindex, controlconnection->Buffer());
                    connection = std::move(controlconnection);
                }
                else
                {
                    throw std::runtime_error("unsupported port type");
                }
            }
            m_Connections.push_back(std::move(connection));
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
        if(m_Plugin.MidiInputIndex())
        {
            if(auto atomconnection = dynamic_cast<TConnection<TAtomPort>*>(m_Connections.at(*m_Plugin.MidiInputIndex()).get()))
            {
                return atomconnection->Buffer();
            }
        }
        return nullptr;
    }
}

