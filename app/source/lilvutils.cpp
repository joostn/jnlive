#include "lilvutils.h"
#include "lv2/port-props/port-props.h"
#include "lv2/atom/atom.h"
#include "lv2/atom/forge.h"
#include "lv2/atom/util.h"
#include "lv2/resize-port/resize-port.h"
#include "lv2_evbuf.h"
#include "lv2/midi/midi.h"
#include "lv2/port-groups/port-groups.h"
#include "suil/suil.h"
#include <filesystem>
#include "lv2_external_ui.h"
#include <iostream>
#include <cstring>
#include "midi.h"

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
            //std::make_tuple(LV2_UI__scaleFactor, &m_OptionUiScaleFactor),
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
        m_BoundedBlockLengthFeature.data = nullptr;
        m_BoundedBlockLengthFeature.URI = LV2_BUF_SIZE__boundedBlockLength;
        m_Features.push_back(&m_BoundedBlockLengthFeature);
        m_PowerOf2BlockLengthFeature.data = nullptr;
        m_PowerOf2BlockLengthFeature.URI = LV2_BUF_SIZE__powerOf2BlockLength;
        m_Features.push_back(&m_PowerOf2BlockLengthFeature);

        // LV2_State_Make_Path m_MakePath;
        // LV2_Feature m_MakePathFeature;
        // m_MakePath.handle = this;
        // m_MakePath.path = [](LV2_State_Make_Path_Handle handle, const char* abstract_path) -> const char* {
        //     auto world = (World*)handle;
        //     return world->MakePath(abstract_path).c_str();
        // };
        // m_MakePathFeature.URI = LV2_STATE__makePath;
        // m_MakePathFeature.data = &m_MakePath;
        // m_Features.push_back(&m_MakePathFeature);
        m_ThreadSafeRestoreFeature.URI = LV2_STATE__threadSafeRestore;
        m_ThreadSafeRestoreFeature.data = nullptr;
        m_Features.push_back(&m_ThreadSafeRestoreFeature);
        lv2_atom_forge_init(&m_AtomForge, &m_UridMap);  
        m_PluginList = BuildPluginList(world);      
        m_World = world;
        m_Plugins = plugins;
        m_SuilHost = suilhost;
        world = nullptr;
        plugins = nullptr;
        suilhost = nullptr;
        staticptr() = this;
    }
    TPluginList World::BuildPluginList(const LilvWorld* world)
    {
        //const LilvPluginClasses *lilvpluginclasses = lilv_world_get_plugin_classes(world); // Returned list is owned by world and must not be freed by the caller
        std::map<const LilvPluginClass*, size_t> class2Index;
        std::vector<TPluginList::TClass> classes;
        auto addpluginclass = [&](const LilvPluginClass *lilvpluginclass){
            std::string uri = lilv_node_as_uri(lilv_plugin_class_get_uri(lilvpluginclass));
            std::string label = lilv_node_as_string(lilv_plugin_class_get_label(lilvpluginclass));
            class2Index.emplace(lilvpluginclass, classes.size());
            classes.emplace_back(std::move(uri), std::move(label));
        };
/*
        LILV_FOREACH(plugin_classes, i, lilvpluginclasses)
        {
            auto lilvpluginclass = lilv_plugin_classes_get(lilvpluginclasses, i);
            if(lilvpluginclass)
            {
                std::string uri = lilv_node_as_uri(lilv_plugin_class_get_uri(lilvpluginclass));
                std::string label = lilv_node_as_string(lilv_plugin_class_get_label(lilvpluginclass));
                class2Index.emplace(lilvpluginclass, classes.size());
                classes.emplace_back(std::move(uri), std::move(label));
            }
        }
    
    */
        auto lilvplugis = lilv_world_get_all_plugins(world); // Returned list is owned by world and must not be freed by the caller
        std::vector<TPluginList::TPlugin> plugins;
        LILV_FOREACH(plugins, i, lilvplugis)
        {
            auto plugin = lilv_plugins_get(lilvplugis, i);
            if(plugin)
            {
                auto lilvpluginclass = lilv_plugin_get_class(plugin);
                auto it = class2Index.find(lilvpluginclass);
                if(it == class2Index.end())
                {
                    addpluginclass(lilvpluginclass);
                }
                auto classindex = class2Index.at(lilvpluginclass);
                std::string uri = lilv_node_as_uri(lilv_plugin_get_uri(plugin));
                std::string label = lilv_node_as_string(lilv_plugin_get_name(plugin));
                plugins.emplace_back(std::move(label), std::move(uri), classindex);
            }
        }
        return TPluginList(std::move(classes), std::move(plugins));
    }

    Plugin::Plugin(const Uri &uri) : m_Uri(uri.str())
    {
        auto lilvworld = World::Static().get();
        auto plugins = World::Static().Plugins();
        auto urinode = uri.get();
        m_Plugin = lilv_plugins_get_by_uri(plugins, urinode); // return value must not be freed
        if(!m_Plugin)
        {
            throw std::runtime_error("could not load plugin: " + m_Uri);
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
        std::vector<size_t> unnamedOutputAudioPorts;
        std::vector<size_t> unnamedInputAudioPorts;
        for(size_t portindex = 0; portindex < m_Ports.size(); portindex++)
        {
            const auto& port = m_Ports[portindex];
            m_PortsBySymbol.emplace(port->Symbol(), port.get());
            if(auto audioport = dynamic_cast<const TAudioPort*>(port.get()); audioport)
            {
                auto &portindices = (audioport->Direction() == TAudioPort::TDirection::Output)? m_AudioOutputIndex:m_AudioInputIndex;
                auto &unnamedportindices = (audioport->Direction() == TAudioPort::TDirection::Output)? unnamedOutputAudioPorts:unnamedInputAudioPorts;
                {
                    if(audioport->Designation() == TAudioPort::TDesignation::StereoLeft)
                    {
                        portindices[0] = portindex;
                    }
                    else if(audioport->Designation() == TAudioPort::TDesignation::StereoRight)
                    {
                        portindices[1] = portindex;
                    }
                    else
                    {
                        unnamedportindices.push_back(portindex);
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
        if(!m_AudioOutputIndex[0])
        {
            if(unnamedOutputAudioPorts.size() >= 1)
            {
                m_AudioOutputIndex[0] = unnamedOutputAudioPorts[0];
            }        
        }
        if(!m_AudioOutputIndex[1])
        {
            if(unnamedOutputAudioPorts.size() >= 2)
            {
                m_AudioOutputIndex[1] = unnamedOutputAudioPorts[1];
            }        
            else if(unnamedOutputAudioPorts.size() >= 1)
            {
                m_AudioOutputIndex[1] = unnamedOutputAudioPorts[0];
            }        
        }
        if(!m_AudioInputIndex[0])
        {
            if(unnamedInputAudioPorts.size() >= 1)
            {
                m_AudioInputIndex[0] = unnamedInputAudioPorts[0];
            }        
        }
        if(!m_AudioInputIndex[1])
        {
            if(unnamedInputAudioPorts.size() >= 2)
            {
                m_AudioInputIndex[1] = unnamedInputAudioPorts[1];
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


    Instance::Instance(const Plugin &plugin, double sample_rate, const RealtimeThreadInterface &realtimeThreadInterface, TMidiCallback &&midiCallback) : m_Plugin(plugin), m_RealtimeThreadInterface(realtimeThreadInterface), m_MidiCallback(std::move(midiCallback))
    {
        if(!plugin.CanInstantiate())
        {
            throw std::runtime_error("plugin "+plugin.Name()+" hasv nsupported ports and cannot be instantiated");
        }
        m_UridMidiEvent = lilvutils::World::Static().UriMapLookup(LV2_MIDI__MidiEvent);
        auto uri_workerinterface = lilvutils::Uri(LV2_WORKER__interface);
        auto uri_threadsaferestore = lilvutils::Uri(LV2_STATE__threadSafeRestore);
        bool needsWorker = lilv_plugin_has_extension_data(plugin.get(),uri_workerinterface.get());
        m_SupportsThreadSafeRestore = lilv_plugin_has_extension_data(plugin.get(), uri_threadsaferestore.get());

        m_Features = World::Static().Features();
        m_Features.push_back(m_Logger.Feature());
        if(needsWorker)
        {
            m_ScheduleWorker = std::make_unique<schedule::Worker>(*this);
            m_Features.push_back(&m_ScheduleWorker->ScheduleFeature());
        }
        m_Features.push_back(nullptr);

        m_StateRestoreFeatures = World::Static().Features();
        m_StateRestoreFeatures.push_back(m_Logger.Feature());
        if(m_SupportsThreadSafeRestore)
        {
            m_StateRestoreWorker = std::make_unique<schedule::Worker>(*this);
            m_StateRestoreFeatures.push_back(&m_ScheduleWorker->ScheduleFeature());
        }
//      &jalv->features.make_path_feature,
//      &jalv->features.safe_restore_feature,
        m_StateRestoreFeatures.push_back(nullptr);

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
    void* Instance::GetPortValueBySymbol(const char *port_symbol, uint32_t *size, uint32_t *type)
    {
        auto port = plugin().PortBySymbol(port_symbol);
        if(port && (port->Direction() == TControlPort::TDirection::Input))
        {
            const auto& connection = Connections().at(port->Index());
            if(auto controlconnection = dynamic_cast<TConnection<TControlPort>*>(connection.get()); controlconnection)
            {
                *size = sizeof(float);
                *type = World::Static().AtomForge().Float;
                // std::cout << "GetPortValueBySymbol: " << port_symbol << " = " << controlconnection->ValueInMainThread() << std::endl;
                return const_cast<void*>((const void*)&controlconnection->ValueInMainThread());
            }
        }
        std::cout << "GetPortValueBySymbol: port not found: " << port_symbol << std::endl;
        return nullptr;
    }


    void Instance::SetPortValueBySymbol(const char *port_symbol, const void *value, uint32_t size, uint32_t type)
    {
        auto port = plugin().PortBySymbol(port_symbol);
        if(port && (port->Direction() == TControlPort::TDirection::Input))
        {
            const auto& connection = Connections().at(port->Index());
            if(auto controlconnection = dynamic_cast<TConnection<TControlPort>*>(connection.get()); controlconnection)
            {
                float fvalue = 0.0f;
                if (type == World::Static().AtomForge().Float)
                {
                    fvalue = *(const float*)value;
                }
                else if (type == World::Static().AtomForge().Double)
                {
                    fvalue = *(const double*)value;
                }
                else if (type == World::Static().AtomForge().Int)
                {
                    fvalue = *(const int32_t*)value;
                }
                else if (type == World::Static().AtomForge().Long)
                {
                    fvalue = *(const int64_t*)value;
                }
                else 
                {
                    throw std::runtime_error("unsupported type");
                }
                realtimeThreadInterface().SendControlValueFunc(controlconnection, fvalue);
                return;
            }
        }
        std::cout << "SetPortValueBySymbol: port not found: " << port_symbol << std::endl;
    }

    void Instance::LoadState(const std::string &dir)
    {
        if(!m_SupportsThreadSafeRestore)
        {
            // suspend audio thread:
        }
        auto statefile = dir + "/state.ttl";
        if(!std::filesystem::exists(statefile))
        {
            throw std::runtime_error("statefile does not exist: "+statefile);
        }
        auto state = lilv_state_new_from_file(World::Static().get(), &World::Static().UridMap(), nullptr, statefile.c_str());
        if(!state)
        {
            throw std::runtime_error("lilv_state_new_from_file failed");
        }
        auto set_port_value = [](const char *port_symbol, void *user_data, const void *value, uint32_t size, uint32_t type) {
            auto self = (Instance*)user_data;
            self->SetPortValueBySymbol(port_symbol, value, size, type);
        };
        uint32_t flags = 0;
        lilv_state_restore(state, m_Instance, set_port_value, this, flags, m_StateRestoreFeatures.data());
    }

    void Instance::SaveState(const std::string &dir)
    {
        // generate a random tempdir in the system temporary dir:
        if(!std::filesystem::exists(dir))
        {
            std::filesystem::create_directory(dir);
        }
        auto tempdir = utils::generate_random_tempdir();
        auto get_port_value = [](const char* port_symbol, void* user_data, uint32_t* size, uint32_t* type) -> const void* {
            auto self = (Instance*)user_data;
            return self->GetPortValueBySymbol(port_symbol, size, type);
        };
        auto state = lilv_state_new_from_instance(m_Plugin.get(), m_Instance, &World::Static().UridMap(), tempdir.c_str(), dir.c_str(), dir.c_str(), dir.c_str(), get_port_value, this, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE, NULL);
        if(!state)
        {
            throw std::runtime_error("lilv_state_new_from_instance failed");
        }
        utils::finally fin1([&](){
            if(state)
            {
                lilv_state_free(state);
            }
        });
        auto status = lilv_state_save(World::Static().get(), &World::Static().UridMap(), &World::Static().UridUnmap(), state, NULL, dir.c_str(), "state.ttl");
        if(status)
        {
            throw std::runtime_error("lilv_state_save failed");
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
        if(type == m_UridMidiEvent)
        {
            if(midi::TMidiOrSysexEvent::IsSupported(data, datasize))
            {
                midi::TMidiOrSysexEvent evt(data, datasize);
                if(m_MidiCallback)
                {
                    m_MidiCallback(evt);
                }
                if(!evt.IsSysex() && (evt.GetSimpleEvent().type() == midi::SimpleEvent::Type::ControlChange))
                {
                    std::cout << evt.ToDebugString() << std::endl;
                }
            }
        }
    }
    
    UI::TUiToShow UI::FindUiToShow(const LilvUIs *uis) const
    {
        UI::TUiToShow result;
        lilvutils::Uri uri_native_ui_type("http://lv2plug.in/ns/extensions/ui#Gtk3UI");
        //LV2_External_UI_Widget* extuiptr = nullptr;
        if(uis)
        {
            for(bool tryExternalUi: {false, true})
            {
                // Try to find a UI with ui:showInterface
                LILV_FOREACH (uis, u, uis) {
                    auto ui = lilv_uis_get(uis, u);
                    auto ui_node = lilv_ui_get_uri(ui);
                    if(ui_node)
                    {
                        lilv_world_load_resource(World::Static().get(), ui_node);
                        utils::finally fin2([&]() {  lilv_world_unload_resource(World::Static().get(), ui_node); });
                        const LilvNode *ui_type = nullptr; // must not be freed by caller
                        bool supported = lilv_ui_is_supported(ui, suil_ui_supported, uri_native_ui_type.get(), &ui_type);
                        if (supported) 
                        {
                            if(!tryExternalUi)
                            {
                                // ok, we have an embeddable UI which can be instantiated as a GTK3 widget:
                                if(!ui_type)
                                {
                                    throw std::runtime_error("ui_type is null");
                                }
                                result.m_Ui = ui;
                                result.m_IsExternalUi = false;
                                result.m_UiTypeUri = lilv_node_as_uri(ui_type);
                                return result;
                            }
                        }
                        else if(tryExternalUi)
                        {
                            auto types = lilv_ui_get_classes(ui); // result must not be freed
                            LILV_FOREACH(nodes, t, types) {
                                auto pt = lilv_node_as_uri(lilv_nodes_get(types, t));
                                if(pt)
                                {
                                    std::string typestr = pt;
                                    if( (typestr == "http://kxstudio.sf.net/ns/lv2ext/external-ui#Widget") || (typestr == "http://lv2plug.in/ns/extensions/ui#external") )
                                    {
                                        result.m_Ui = ui;
                                        result.m_IsExternalUi = true;
                                        result.m_UiTypeUri = typestr;
                                        return result;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        return TUiToShow{}; // with m_Ui == nullptr
    }

    UI::UI(Instance &instance) : m_Instance(instance)
    {
        auto uis = lilv_plugin_get_uis(m_Instance.plugin().get());
        utils::finally fin1([&]() {  if(uis) lilv_uis_free(uis); });
        auto uiToShow = FindUiToShow(uis);
        if(!uiToShow.m_Ui)
        {
            throw std::runtime_error("No suitable UI found");
        }
        m_Uridatom_eventTransfer = lilvutils::World::Static().UriMapLookup(LV2_ATOM__eventTransfer);
        auto features = World::Static().Features();
        ContainerWindow* containerWindow = nullptr;
        utils::finally fin5([&]() { if(containerWindow) delete containerWindow; });
        LV2_Feature uiParentFeature;
        if(!uiToShow.m_IsExternalUi)
        {
            containerWindow = new ContainerWindow(*this);
            uiParentFeature.URI = LV2_UI__parent;
            uiParentFeature.data = containerWindow;
            features.push_back(&uiParentFeature);
        }
        LV2_Feature instanceAccessFeature;
        instanceAccessFeature.URI = LV2_INSTANCE_ACCESS_URI;
        instanceAccessFeature.data = m_Instance.Handle();
        features.push_back(&instanceAccessFeature);
        auto extensiondata = lilv_instance_get_descriptor(m_Instance.get())->extension_data;
        LV2_Feature dataAccessFeature;
        dataAccessFeature.URI = LV2_DATA_ACCESS_URI;
        dataAccessFeature.data = const_cast<void*>((const void*)extensiondata);
        features.push_back(&dataAccessFeature);
        LV2_Feature idleFeature;
        idleFeature.URI = LV2_UI__idleInterface;
        idleFeature.data = nullptr;
        features.push_back(&idleFeature);
        LV2_Feature requestValueFeature;
        requestValueFeature.URI = LV2_UI__requestValue;
        requestValueFeature.data = &m_RequestValue;
        m_RequestValue.handle = this;
        m_RequestValue.request = [](LV2UI_Feature_Handle handle, LV2_URID key, LV2_URID type, const LV2_Feature *const *features) {
            return static_cast<UI*>(handle)->RequestValue(key, type, features);
        };
        features.push_back(m_Logger.Feature());
        m_ExternalUiHost.ui_closed = [](LV2UI_Controller controller){
            auto self = (UI*)controller;
            if(self)
            {
                if(!self->m_OnCloseNoRecurse)
                {
                    self->m_OnCloseNoRecurse = true;
                    utils::finally fin1([&](){
                        self->m_OnCloseNoRecurse = false;
                    });
                    self->m_ExternalUiWidget = nullptr;
                    self->OnClose().Notify();
                }
            }
        };
        auto human_id = m_Instance.plugin().Name();
        m_ExternalUiHost.plugin_human_id = human_id.c_str();
		LV2_Feature external_lv_feature = { LV2_EXTERNAL_UI_DEPRECATED_URI, &m_ExternalUiHost };
        features.push_back(&external_lv_feature);
		LV2_Feature external_kx_feature = { LV2_EXTERNAL_UI__Host, &m_ExternalUiHost };
        features.push_back(&external_kx_feature);
        features.push_back(nullptr);

        std::string container_type_uri;
        if(uiToShow.m_IsExternalUi)
        {
            container_type_uri = uiToShow.m_UiTypeUri;
        }
        else
        {
            container_type_uri = "http://lv2plug.in/ns/extensions/ui#Gtk3UI";
        }
        auto bundle_uri  = lilv_node_as_uri(lilv_ui_get_bundle_uri(uiToShow.m_Ui));
        auto binary_uri  = lilv_node_as_uri(lilv_ui_get_binary_uri(uiToShow.m_Ui));
        auto bundle_path = lilv_file_uri_parse(bundle_uri, NULL);
        utils::finally fin3([&]() { if(bundle_path) lilv_free(bundle_path); });
        auto binary_path = lilv_file_uri_parse(binary_uri, NULL);
        utils::finally fin4([&]() { if(binary_path) lilv_free(binary_path); });
        auto pluginuri = m_Instance.plugin().Lv2Uri();
        auto ui_node = lilv_ui_get_uri(uiToShow.m_Ui);
        if(!ui_node)
        {
            throw std::runtime_error("ui_node is null");
        }
        auto ui_node_char = lilv_node_as_uri(ui_node);
        if(!ui_node_char)
        {
            throw std::runtime_error("ui_node_char is null");
        }
        auto suilinstance = suil_instance_new(World::Static().suilHost(), this, container_type_uri.c_str(), pluginuri.c_str(), ui_node_char, uiToShow.m_UiTypeUri.c_str(), bundle_path, binary_path, features.data());
        utils::finally fin6([&]() { if(suilinstance)  suil_instance_free(suilinstance); });
        if(!suilinstance)
        {
            throw std::runtime_error("suil_instance_new failed");
        }
        auto widget = suil_instance_get_widget(suilinstance);        
        if(!widget)
        {
            throw std::runtime_error("suil_instance_get_widget failed");
        }
        if(uiToShow.m_IsExternalUi)
        {
            m_ExternalUiWidget = (LV2_External_UI_Widget*)widget;
            m_ExternalUiWidget->show(m_ExternalUiWidget);
        }
        else
        {
            auto gtkwidget = (GtkWidget*)widget;
            containerWindow->AttachWidget(gtkwidget);
            containerWindow->present();
            containerWindow->show_all();
        }
        m_SuilInstance = suilinstance;
        m_Ui = uiToShow.m_Ui;
        m_ContainerWindow = containerWindow;
        suilinstance = nullptr;
        containerWindow = nullptr;
        m_Instance.UiOpened(this);
    }

    UI::~UI()
    {
        m_Instance.UiClosed(this);
        if(m_ExternalUiWidget)
        {
            if(!m_OnCloseNoRecurse)
            {
                m_OnCloseNoRecurse = true;
                utils::finally fin1([&](){
                    m_OnCloseNoRecurse = false;
                });
                m_ExternalUiWidget->hide(m_ExternalUiWidget);
            }
            m_ExternalUiWidget = nullptr;
        }
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
    void UI::OnAtomPortMessage(uint32_t portindex, LV2_URID type, uint32_t datasize, const void *data)
    {
        if(m_SuilInstance)
        {
            std::vector<uint8_t> buffer(datasize + sizeof(LV2_Atom));
            auto atom = (LV2_Atom*)buffer.data();
            atom->size = datasize;
            atom->type = type;
            memcpy(atom + 1, data, datasize);
            suil_instance_port_event(m_SuilInstance, portindex, buffer.size(), m_Uridatom_eventTransfer, buffer.data());
        }
    }

    void lilvutils::UI::ContainerWindow::Show(bool show)
    {
        m_NoHideSignalRecurse = true;
        utils::finally fin([&](){
            m_NoHideSignalRecurse = false;
        });
        set_visible(show);
    }

    lilvutils::UI::ContainerWindow::ContainerWindow(UI &ui) : m_UI(ui), Gtk::Window(Gtk::WINDOW_TOPLEVEL)
    {
        set_resizable(true);
        signal_hide().connect([&](){
            if(!m_NoHideSignalRecurse)
            {
                m_NoHideSignalRecurse = true;
                utils::finally fin([&](){
                    m_NoHideSignalRecurse = false;
                });
                m_UI.OnClose().Notify();
            }
        });

        auto vbox = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_VERTICAL, 0);
        add(*vbox);

        m_Container = Gtk::make_managed<Gtk::EventBox>();
        m_Container->set_halign(Gtk::ALIGN_FILL);
        m_Container->set_hexpand(true);
        m_Container->set_valign(Gtk::ALIGN_FILL);
        m_Container->set_vexpand(true);
        vbox->pack_start(*m_Container, true, true, 0);
        m_Container->show();
        vbox->show_all();

    }

    void UI::CallIdle()
    {
        auto now = std::chrono::steady_clock::now();
        if(now > m_LastExternalUiRunCall + std::chrono::milliseconds(30))
        {
            m_LastExternalUiRunCall = now;
            if(m_ExternalUiWidget)
            {
                m_ExternalUiWidget->run(m_ExternalUiWidget);
            }
        }
    }

    void UI::OnControlValueChanged(uint32_t portindex, float value)
    {
        if(m_SuilInstance)
        {
            suil_instance_port_event(m_SuilInstance, portindex, 4, 0, &value);
        }
    }
    uint32_t UI::PortIndex(const char *port_symbol) const
    {
        std::string port_symbol_str(port_symbol);
        auto port = m_Instance.plugin().PortBySymbol(port_symbol_str);
        if(port)
        {
            return (uint32_t)port->Index();
        }
        return LV2UI_INVALID_PORT_INDEX; 
    }
    void UI::PortWrite(uint32_t port_index, uint32_t buffer_size, uint32_t protocol, void const *buffer)
    {
        if(port_index < m_Instance.Connections().size())
        {
            auto connection = m_Instance.Connections().at(port_index).get();
            if(protocol == 0)
            {
                if(auto controlconnection = dynamic_cast<TConnection<TControlPort>*>(connection))
                {
                    float newvalue = *(float*)buffer;
                    controlconnection->SetValueInMainThread(newvalue);
                    instance().realtimeThreadInterface().SendControlValueFunc(controlconnection, newvalue);
                    return;
                }
            }
            else if (protocol == m_Uridatom_eventTransfer)
            {
                if(auto atomconnection = dynamic_cast<TConnection<TAtomPort>*>(connection))
                {
                    auto  atom = (const LV2_Atom*)buffer;
                    if (buffer_size > sizeof(LV2_Atom)) 
                    {
                        if (sizeof(LV2_Atom) + atom->size == buffer_size)
                        {
                            instance().realtimeThreadInterface().SendAtomPortEventFunc(atomconnection, 0, 0, atom->type, atom->size, atom + 1U);
                            return;
                        }
                    }
                }
            }
        }
        std::cout << "UI::PortWrite: failed" << std::endl;

    }

    LV2UI_Request_Value_Status UI::RequestValue(LV2_URID key, LV2_URID type, const LV2_Feature *const *features)
    {
        return LV2UI_REQUEST_VALUE_ERR_UNSUPPORTED;
    }
    bool UI::IsShown() const
    {
        if(m_ExternalUiWidget)
        {
            return true;
        }
        else
        {
            return m_ContainerWindow->get_visible();
        }
    }
    void UI::SetShown(bool shown)
    {
        if(m_ContainerWindow)
        {
            m_ContainerWindow->Show(shown);
        }
    }
    bool UI::CanHide() const
    {
        return m_ContainerWindow;
    }
}

