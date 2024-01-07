#include "lilvutils.h"
#include "lv2/port-props/port-props.h"
#include "lv2/atom/atom.h"
#include "lv2/resize-port/resize-port.h"
#include "lv2_evbuf.h"
#include "lv2/midi/midi.h"
#include "lv2/port-groups/port-groups.h"
#include "suil/suil.h"

namespace 
{
    uint32_t SuilPortIndex(SuilController controller, const char *port_symbol)
    {
        auto ui = (lilvutils::UI *)(controller);
        if(ui)
        {
            return ui->PortIndex(port_symbol);
        }
        return LV2UI_INVALID_PORT_INDEX;
    }

/*
    uint32_t SuilPortSubscribe(SuilController controller, uint32_t port_index, uint32_t protocol, const LV2_Feature *const *features)
    {
        return 0;

    }

    uint32_t SuilPortUnsubscribe(SuilController controller, uint32_t port_index, uint32_t protocol, const LV2_Feature *const *features)
    {
        return 0;

    }
*/
    void SuilPortWrite(SuilController controller, uint32_t port_index, uint32_t buffer_size, uint32_t protocol, void const *buffer)
    {
        auto ui = (lilvutils::UI *)(controller);
        if(ui)
        {
            ui->PortWrite(port_index, buffer_size, protocol, buffer);
        }
    }

    void SuilTouch(SuilController controller, uint32_t port_index, bool grabbed)
    {

    }

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
                portbase = std::make_unique<lilvutils::TAudioPort>(portindex, direction, std::move(name), std::move(symbol), designation, optional);
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
                portbase = std::make_unique<lilvutils::TAtomPort>(portindex, direction, std::move(name), std::move(symbol), designation, supportsmidi, minbufsize, optional);
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
                    portbase = std::make_unique<lilvutils::TControlPort>(portindex, direction, std::move(name), std::move(symbol), minv, maxv, defv, optional);
                }
            }
            else if(iscvport)
            {
                portbase = std::make_unique<lilvutils::TCvPort>(portindex, direction, std::move(name), std::move(symbol), optional);
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
    World::World(uint32_t sample_rate, uint32_t maxBlockSize, int argc, char** argv) : m_OptionSampleRate((float)sample_rate), m_OptionMaxBlockLength(maxBlockSize)
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
        suil_init(&argc, &argv, SUIL_ARG_NONE);
        // auto suilhost = suil_host_new(&SuilPortWrite, &SuilPortIndex, &SuilPortSubscribe, &SuilPortUnsubscribe);
        auto suilhost = suil_host_new(&SuilPortWrite, &SuilPortIndex, nullptr, nullptr);
        utils::finally fin2([&](){
            if(suilhost)
            {
                suil_host_free(suilhost); 
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
        m_World = world;
        m_Plugins = plugins;
        m_SuilHost = suilhost;
        world = nullptr;
        plugins = nullptr;
        suilhost = nullptr;
        staticptr() = this;
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

    void TConnection<TControlPort>::SetValueInMainThread(const float& valueInMainThread, bool notify) 
    { 
        m_ValueInMainThread = valueInMainThread;
        if(notify)
        {
            m_Instance.OnControlValueChanged(*this);
        }
    }


    Instance::Instance(const Plugin &plugin, double sample_rate, const RealtimeThreadInterface &realtimeThreadInterface) : m_Plugin(plugin), m_RealtimeThreadInterface(realtimeThreadInterface)
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
                if(auto audioport = dynamic_cast<const TAudioPort*>(port); audioport)
                {
                    auto audioconnection = std::make_unique<TConnection<TAudioPort>>(*this, *audioport, audiobufsize);
                    lilv_instance_connect_port(m_Instance, (uint32_t)portindex, audioconnection->Buffer());
                    connection = std::move(audioconnection);
                }
                else if(auto cvport = dynamic_cast<const TCvPort*>(port); cvport)
                {
                    auto audioconnection = std::make_unique<TConnection<TCvPort>>(*this, *cvport, audiobufsize);
                    lilv_instance_connect_port(m_Instance, (uint32_t)portindex, audioconnection->Buffer());
                    connection = std::move(audioconnection);
                }
                else if(auto atomport = dynamic_cast<const TAtomPort*>(port); atomport)
                {
                    m_PortIndicesOfAtomPorts.push_back(portindex);
                    auto bufsize = atomport->MinBufferSize().value_or(0);
                    bufsize = std::max(bufsize, 65536);
                    auto atomconnection = std::make_unique<TConnection<TAtomPort>>(*this, *atomport, (uint32_t)bufsize);
                    lv2_evbuf_reset(atomconnection->Buffer(), atomport->Direction() == TAtomPort::TDirection::Input);
                    auto actualbuf = lv2_evbuf_get_buffer(atomconnection->Buffer());
                    lilv_instance_connect_port(m_Instance, (uint32_t)portindex, actualbuf);
                    connection = std::move(atomconnection);
                }
                else if(auto controlport = dynamic_cast<const TControlPort*>(port); controlport)
                {
                    auto controlconnection = std::make_unique<TConnection<TControlPort>>(*this, *controlport, controlport->DefaultValue());
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

    void Instance::OnControlValueChanged(TConnection<TControlPort> &connection)
    {
        if(m_Ui)
        {
            m_Ui->OnControlValueChanged(connection.Port().Index(), connection.ValueInMainThread());
        }
    }

    void Instance::OnAtomPortMessage(TConnection<TAtomPort> &connection, uint32_t frames, uint32_t subframes, LV2_URID type, uint32_t datasize, const void *data)
    {
        if(m_Ui)
        {
            m_Ui->OnAtomPortMessage(connection.Port().Index(), type, datasize, data);
        }
    }

    UI::UI(Instance &instance) : m_Instance(instance)
    {
        m_Uridatom_eventTransfer = lilvutils::World::Static().UriMapLookup(LV2_ATOM__eventTransfer);
        auto containerWindow = new ContainerWindow(*this);
        utils::finally fin5([&]() { if(containerWindow) delete containerWindow; });
        m_InstanceAccessFeature.URI = LV2_INSTANCE_ACCESS_URI;
        m_InstanceAccessFeature.data = m_Instance.Handle();
        m_UiParentFeature.URI = LV2_UI__parent;
        m_UiParentFeature.data = nullptr;

        auto extensiondata = lilv_instance_get_descriptor(m_Instance.get())->extension_data;
        m_DataAccessFeature.URI = LV2_DATA_ACCESS_URI;
        m_DataAccessFeature.data = const_cast<void*>((const void*)extensiondata);
        m_IdleFeature.URI = LV2_UI__idleInterface;
        m_IdleFeature.data = nullptr;
        m_RequestValueFeature.URI = LV2_UI__requestValue;
        m_RequestValueFeature.data = &m_RequestValue;
        m_RequestValue.handle = this;
        m_RequestValue.request = [](LV2UI_Feature_Handle handle, LV2_URID key, LV2_URID type, const LV2_Feature *const *features) {
            return static_cast<UI*>(handle)->RequestValue(key, type, features);
        };
        GtkWindow *x;
        m_Features = World::Static().Features();
        m_Features.push_back(m_Logger.Feature());
        m_Features.push_back(&m_InstanceAccessFeature);
        m_Features.push_back(&m_UiParentFeature);
        m_Features.push_back(&m_DataAccessFeature);
        m_Features.push_back(&m_IdleFeature);
        const LV2_Feature parent_feature = {LV2_UI__parent, containerWindow->Container()};
        m_Features.push_back(&parent_feature);
        m_Features.push_back(nullptr);
        lilvutils::Uri uri_extensionData(LV2_CORE__extensionData);
        lilvutils::Uri uri_showInterface(LV2_UI__showInterface);

        const char* container_type_uri = "http://lv2plug.in/ns/extensions/ui#Gtk3UI";
        lilvutils::Uri uri_native_ui_type(container_type_uri);


        const LilvUI* uiToShow = nullptr;
        auto uis = lilv_plugin_get_uis(m_Instance.plugin().get());
        utils::finally fin1([&]() {  if(uis) lilv_uis_free(uis); });
        const LilvNode *ui_type = nullptr; // must not be freed by caller
        if(uis)
        {
            // Try to find a UI with ui:showInterface
            LILV_FOREACH (uis, u, uis) {
                const LilvUI*   ui      = lilv_uis_get(uis, u);
                const LilvNode* ui_node = lilv_ui_get_uri(ui);

                lilv_world_load_resource(World::Static().get(), ui_node);
                utils::finally fin2([&]() {  lilv_world_unload_resource(World::Static().get(), ui_node); });

                //bool supported = lilv_world_ask(World::Static().get(), ui_node, uri_extensionData.get(), uri_showInterface.get());
                ui_type = nullptr; // must not be freed by caller
                bool supported = lilv_ui_is_supported(ui, suil_ui_supported, uri_native_ui_type.get(), &ui_type);

                if (supported) 
                {
                    uiToShow = ui;
                    break;
                }
            }
        }
        if(!uiToShow)
        {
            throw std::runtime_error("No UI found");
        }

        auto bundle_uri  = lilv_node_as_uri(lilv_ui_get_bundle_uri(uiToShow));
        auto binary_uri  = lilv_node_as_uri(lilv_ui_get_binary_uri(uiToShow));
        auto bundle_path = lilv_file_uri_parse(bundle_uri, NULL);
        utils::finally fin3([&]() { if(bundle_path) lilv_free(bundle_path); });
        auto binary_path = lilv_file_uri_parse(binary_uri, NULL);
        utils::finally fin4([&]() { if(binary_path) lilv_free(binary_path); });
        auto suilinstance = suil_instance_new(World::Static().suilHost(), this, container_type_uri, lilv_node_as_uri(lilv_plugin_get_uri(m_Instance.plugin().get())), lilv_node_as_uri(lilv_ui_get_uri(uiToShow)), lilv_node_as_uri(ui_type), bundle_path, binary_path, m_Features.data());
        utils::finally fin6([&]() { if(suilinstance)  suil_instance_free(suilinstance); });
        if(!suilinstance)
        {
            throw std::runtime_error("suil_instance_new failed");
        }
        auto widget = (GtkWidget*)suil_instance_get_widget(suilinstance);
        
        if(!suilinstance)
        {
            throw std::runtime_error("suil_instance_get_widget failed");
        }
        containerWindow->AttachWidget(widget);
        containerWindow->present();
        containerWindow->show_all();

        m_SuilInstance = suilinstance;
        m_Ui = uiToShow;
        m_ContainerWindow = containerWindow;
        suilinstance = nullptr;
        uiToShow = nullptr;
        containerWindow = nullptr;

        m_Instance.UiOpened(this);
    }

    UI::~UI()
    {
        m_Instance.UiClosed(this);
        if(m_ContainerWindow)
        {
            m_ContainerWindow->RemoveWidget();
        }
        if(m_SuilInstance)
        {
            suil_instance_free(m_SuilInstance);
            m_SuilInstance = nullptr;
        }
        if(m_ContainerWindow)
        {
            delete m_ContainerWindow;
            m_ContainerWindow = nullptr;
        }

    }
}

