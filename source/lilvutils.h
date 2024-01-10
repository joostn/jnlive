#pragma once

#include <stdexcept>	
#include "utils.h"
#include "schedule.h"
#include <lilv/lilv.h>
#include <mutex>
#include <optional>
#include <map>
#include <functional>
#include "lv2/atom/atom.h"
#include "lv2/atom/forge.h"
#include "lv2_evbuf.h"
#include "log.h"
#include "lv2/log/log.h"
#include "lv2/options/options.h"
#include "lv2/parameters/parameters.h"
#include "lv2/ui/ui.h"
#include "lv2/buf-size/buf-size.h"
#include "suil/suil.h"
#include "lv2/instance-access/instance-access.h"
#include "lv2/data-access/data-access.h"
#include <gtkmm.h>

namespace lilvutils
{
    class Instance;
    class UI;
    class World
    {
    public:
        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = delete;
        World& operator=(World&&) = delete;
        World(uint32_t sample_rate, uint32_t maxBlockSize, int argc, char** argv);
        ~World()
        {
            if(m_World) lilv_world_free(m_World);
            if(m_SuilHost) suil_host_free(m_SuilHost); 
            staticptr() = nullptr;
        }        
        const LilvPlugins* Plugins()
        {
            return m_Plugins;
        }
        LilvWorld* get()
        {
            return m_World;
        }
        static World& Static()
        {
            if(!staticptr())
            {
                throw std::runtime_error("lilv_world not instantiated");
            }
            return *staticptr();
        }
        LV2_URID UriMapLookup(const std::string &str)
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            auto it = m_UriMap.find(str);
            if(it != m_UriMap.end())
            {
                return (LV2_URID)it->second;
            }
            else
            {
                auto index = m_UriMapReverse.size();
                m_UriMap[str] = index;
                m_UriMapReverse.push_back(str);
                if(index > std::numeric_limits<LV2_URID>::max())
                {
                    throw std::runtime_error("LV2_URID overflow");
                }
                return (LV2_URID)index;
            }
        }
        const std::string & UriMapReverseLookup(size_t index)
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            return m_UriMapReverse.at(index);
        }
        const std::vector<const LV2_Feature*>& Features() const { return m_Features; }
        uint32_t MaxBlockLength() const { return m_OptionMaxBlockLength; }
        SuilHost* suilHost() const { return m_SuilHost; }
        LV2_URID_Map& UridMap() { return m_UridMap; }   
        LV2_URID_Unmap& UridUnmap() { return m_UridUnmap; }
        LV2_Atom_Forge& AtomForge() { return m_AtomForge; }

    private:
        static World*& staticptr()
        {
            static World *s_World = nullptr;
            return s_World;
        }

    private:
        std::mutex m_Mutex;
        std::map<std::string, size_t> m_UriMap;
        std::vector<std::string> m_UriMapReverse;
        LilvWorld* m_World = nullptr;
        SuilHost* m_SuilHost = nullptr;
        const LilvPlugins *m_Plugins = nullptr;
        LV2_URID_Map      m_UridMap;
        LV2_URID_Unmap    m_UridUnmap;
        LV2_Feature m_MapFeature;
        LV2_Feature m_UnmapFeature;
        LV2_Feature m_OptionsFeature;
        LV2_Feature m_ThreadSafeRestoreFeature;
        //LV2_State_Make_Path m_MakePath;
        //LV2_Feature m_MakePathFeature;
        
        std::vector<const LV2_Feature*> m_Features;
        std::vector<LV2_Options_Option> m_Options;
        float m_OptionSampleRate = 0.0f;
        uint32_t m_OptionMinBlockLength = 16;
        uint32_t m_OptionMaxBlockLength = 4096;
        uint32_t m_OptionSequenceSize = 4096;
        float m_OptionUiUpdateRate = 30.0f;
        float m_OptionUiScaleFactor = 2.0f;
        LV2_Atom_Forge m_AtomForge;
    };
    class Uri
    {
    public:
        Uri(const Uri&) = delete;
        Uri& operator=(const Uri&) = delete;
        Uri(Uri&&) = delete;
        Uri& operator=(Uri&&) = delete;
        Uri(std::string &&name) : m_Name(std::move(name))
        {
            m_node = lilv_new_uri(World::Static().get(), m_Name.c_str());
            if(!m_node)
            {
                throw std::runtime_error("lilv_new_uri ("+m_Name+") failed");
            }
        }
        ~Uri()
        {
            if(m_node)
            {
                lilv_node_free(m_node);
            }
        }
        const LilvNode* get() const { return m_node; }
        const std::string& str() const { return m_Name; }
    private:
        std::string m_Name;
        LilvNode *m_node = nullptr;
    };
    class TPortBase
    {
    public:
        enum class TDirection {Input, Output};
        TPortBase(size_t index, TDirection direction, std::string &&name, std::string &&symbol, bool optional) : m_Direction(direction), m_Name(std::move(name)), m_Symbol(std::move(symbol)), m_Optional(optional), m_Index(index) {}
        virtual ~TPortBase() {}
        TDirection Direction() const { return m_Direction; }
        const std::string& Name() const { return m_Name; }
        const std::string& Symbol() const { return m_Symbol; }
        bool Optional() const { return m_Optional; }
        size_t Index() const { return m_Index; }
    private:
        size_t m_Index;
        TDirection m_Direction;
        std::string m_Name;
        std::string m_Symbol;
        bool m_Optional;
    };
    class TAudioPort : public TPortBase
    {
    public:
        enum class TDesignation {StereoLeft, StereoRight};
        TAudioPort(size_t index, TDirection direction, std::string &&name, std::string &&symbol, std::optional<TDesignation> designation, bool optional) : TPortBase(index, direction, std::move(name), std::move(symbol), optional), m_Designation(designation) {}
        virtual ~TAudioPort() {}
        std::optional<TDesignation> Designation() const { return m_Designation; }
    private:
        std::optional<TDesignation> m_Designation;
    };
    class TCvPort : public TPortBase
    {
    public:
        TCvPort(size_t index, TDirection direction, std::string &&name, std::string &&symbol, bool optional) : TPortBase(index, direction, std::move(name), std::move(symbol), optional) {}
        virtual ~TCvPort() {}
    };
    class TAtomPort : public TPortBase
    {
    public:
        enum class TDesignation {Control};
        TAtomPort(size_t index, TDirection direction, std::string &&name, std::string &&symbol, std::optional<TDesignation> designation, bool supportsmidi, const std::optional<int> &minbuffersize, bool optional) : TPortBase(index, direction, std::move(name), std::move(symbol), optional), m_Designation(designation), m_SupportsMidi(supportsmidi), m_MinBufferSize(minbuffersize) {}
        virtual ~TAtomPort() {}
        std::optional<TDesignation> Designation() const { return m_Designation; }
        bool SupportsMidi() const { return m_SupportsMidi; }
        const std::optional<int>& MinBufferSize() const { return m_MinBufferSize; }
    private:
        std::optional<TDesignation> m_Designation;
        bool m_SupportsMidi;
        std::optional<int> m_MinBufferSize;
    };
    class TControlPort : public TPortBase
    {
    public:
        TControlPort(size_t index, TDirection direction, std::string &&name, std::string &&symbol, float minimum, float maximum, float defaultvalue, bool optional) : TPortBase(index, direction, std::move(name), std::move(symbol), optional), m_Minimum(minimum), m_Maximum(maximum), m_DefaultValue(defaultvalue) {}
        virtual ~TControlPort() {}
        float Minimum() const { return m_Minimum; }
        float Maximum() const { return m_Maximum; }
        float DefaultValue() const { return m_DefaultValue; }
    private:
        float m_Minimum;
        float m_Maximum;
        float m_DefaultValue;
    };
    class Plugin
    {
    public:
        Plugin(const Plugin&) = delete;
        Plugin& operator=(const Plugin&) = delete;
        Plugin(Plugin&&) = delete;
        Plugin& operator=(Plugin&&) = delete;
        Plugin(const Uri &uri);
        ~Plugin()
        {
            // no need to free
        }
        const LilvPlugin* get() const { return m_Plugin; }
        const std::optional<uint32_t>& MidiInputIndex() const { return m_MidiInputIndex; }
        const std::array<std::optional<uint32_t>,2>& OutputAudioPortIndices() const { return m_AudioOutputIndex; }
        const std::string& Lv2Uri() const { return m_Uri; }
        const std::vector<std::unique_ptr<TPortBase>>& Ports() const { return m_Ports; }
        bool CanInstantiate() const { return m_CanInstantiate; }
        const std::string& Name() const { return m_Name; }
        const TPortBase* PortBySymbol(const std::string &name) const
        {
            auto it = m_PortsBySymbol.find(name);
            if(it != m_PortsBySymbol.end())
            {
                return it->second;
            }
            else
            {
                return nullptr;
            }
        }

    private:
        std::string m_Uri;
        std::string m_Name;
        const LilvPlugin *m_Plugin = nullptr;
        std::optional<uint32_t> m_MidiInputIndex;
        std::vector<std::unique_ptr<TPortBase>> m_Ports;
        std::map<std::string, TPortBase*> m_PortsBySymbol;
        bool m_CanInstantiate = false;
        std::array<std::optional<uint32_t>,2> m_AudioOutputIndex;
    };
    class TConnectionBase
    {
    public:
        TConnectionBase() {}
        virtual ~TConnectionBase() {}
    };
    template <class TPort> class TConnection;
    template <>
    class TConnection<TAudioPort> : public TConnectionBase
    {
    public:
        TConnection(Instance &instance, const TAudioPort &port, size_t bufsize) : m_Instance(instance), m_Buffer(bufsize), m_Port(port)
        {
            std::fill(m_Buffer.begin(), m_Buffer.end(), 0.0f);
        }
        virtual ~TConnection() {}
        float *Buffer() { return &m_Buffer[0]; }
        const TAudioPort& Port() const { return m_Port; }
        Instance &instance() const {return m_Instance;}
    private:
        std::vector<float> m_Buffer;
        const TAudioPort &m_Port;
        Instance &m_Instance;
    };
    template <>
    class TConnection<TCvPort> : public TConnectionBase
    {
    public:
        TConnection(Instance &instance, const TCvPort &port, size_t bufsize) : m_Buffer(bufsize), m_Port(port), m_Instance(instance)
        {
            std::fill(m_Buffer.begin(), m_Buffer.end(), 0.0f);
        }
        virtual ~TConnection() {}
        float *Buffer() { return &m_Buffer[0]; }
        const TCvPort& Port() const { return m_Port; }
        Instance &instance() const {return m_Instance;}
    private:
        std::vector<float> m_Buffer;
        const TCvPort& m_Port;
        Instance &m_Instance;
    };
    template <>
    class TConnection<TAtomPort> : public TConnectionBase
    {
    public:
        TConnection(Instance &instance, const TAtomPort &port, uint32_t bufsize) : m_Port(port), m_Instance(instance)
        {
            auto urid_chunk = lilvutils::World::Static().UriMapLookup(LV2_ATOM__Chunk);
            auto urid_sequence = lilvutils::World::Static().UriMapLookup(LV2_ATOM__Sequence);
            m_EvBuf = lv2_evbuf_new(bufsize, urid_chunk, urid_sequence);
            ResetEvBuf();
        }
        virtual ~TConnection()
        {
            if(m_EvBuf)
            {
                lv2_evbuf_free(m_EvBuf);
            }
        }
        LV2_Evbuf* Buffer() const { return m_EvBuf; }
        Instance &instance() const {return m_Instance;}
        const TAtomPort& Port() const { return m_Port; }
        LV2_Evbuf_Iterator& BufferIterator() { return m_EvBufIterator; }
        void ResetEvBuf()
        {
            bool isinput = Port().Direction() == lilvutils::TPortBase::TDirection::Input;
            lv2_evbuf_reset(Buffer(), isinput);
            BufferIterator() = lv2_evbuf_begin(Buffer());

        }
    private:
        LV2_Evbuf* m_EvBuf = nullptr;
        LV2_Evbuf_Iterator m_EvBufIterator;
        const TAtomPort &m_Port;
        Instance &m_Instance;
    };
    template <>
    class TConnection<TControlPort> : public TConnectionBase
    {
    public:
        TConnection(Instance &instance, const TControlPort &port, const float &defaultvalue) : m_Value(defaultvalue), m_OrigValue(defaultvalue), m_Instance(instance), m_ValueInMainThread(defaultvalue), m_Port(port)
        {
        }
        virtual ~TConnection() {}
        float* Buffer() { return &m_Value; }
        float& OrigValue() { return m_OrigValue;}
        Instance &instance() const {return m_Instance;}
        const float& ValueInMainThread() const { return m_ValueInMainThread; }
        void SetValueInMainThread(const float& valueInMainThread, bool notify);
        const TControlPort& Port() const { return m_Port; }
    private:
        float m_Value;      // accessed from realtime thread
        float m_OrigValue; // accessed from realtime thread
        float m_ValueInMainThread; // accessed from main thread
        const TControlPort& m_Port;
        Instance &m_Instance;
    };
    struct RealtimeThreadInterface
    {
        /* interface for sending notifications to the plugin instance in the audio processing thread. These should be implemented externally, and passed to the constructor of Instance. */
        std::function<void (lilvutils::TConnection<lilvutils::TAtomPort>* connection, uint32_t frames, uint32_t subframes, LV2_URID type, uint32_t size, const void* body)> SendAtomPortEventFunc;
        std::function<void (lilvutils::TConnection<lilvutils::TControlPort>* connection, float value)> SendControlValueFunc;
    };

    class Instance
    {
    public:
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&&) = delete;
        Instance& operator=(Instance&&) = delete;
        Instance(const Plugin &plugin, double sample_rate, const RealtimeThreadInterface &realtimeThreadInterface);
        ~Instance();
        void OnControlValueChanged(TConnection<TControlPort> &connection);
        void OnAtomPortMessage(TConnection<TAtomPort> &connection, uint32_t frames, uint32_t subframes, LV2_URID type, uint32_t datasize, const void *data);
        void Run(uint32_t samplecount)
        {
            lilv_instance_run(m_Instance, samplecount);
            if(m_ScheduleWorker)
            {
                m_ScheduleWorker->RunInRealtimeThread();
            }
        }
        const LilvInstance* get() const { return m_Instance; }
        LV2_Handle Handle() const { return lilv_instance_get_handle(m_Instance); }
        LilvInstance* get() { return m_Instance; }
        const Plugin& plugin() const { return m_Plugin; }
        const std::vector<std::unique_ptr<TConnectionBase>>& Connections() const { return m_Connections; }
        void UiOpened(UI *ui)
        {
            if(m_Ui) throw std::runtime_error("UI already opened");
            m_Ui = ui;
        }
        void UiClosed(UI *ui)
        {
            if(m_Ui != ui) throw std::runtime_error("UI not opened");
            m_Ui = nullptr;
        }
        const RealtimeThreadInterface& realtimeThreadInterface() const { return m_RealtimeThreadInterface; }
        void Reset()
        {
            if(m_Ui) throw std::runtime_error("UI is open");
            lilv_instance_deactivate(m_Instance);
            lilv_instance_activate(m_Instance);
        }
        void SaveState(const std::string &dir);
        void LoadState(const std::string &dir);

    private:
        void* GetPortValueBySymbol(const char *port_symbol, uint32_t *size, uint32_t *type);
        void SetPortValueBySymbol(const char *port_symbol, const void *value, uint32_t size, uint32_t type);

    private:
        const Plugin &m_Plugin;
        std::vector<const LV2_Feature*> m_Features;
        std::vector<const LV2_Feature*> m_StateRestoreFeatures;
        LilvInstance *m_Instance = nullptr;
        std::vector<std::unique_ptr<TConnectionBase>> m_Connections;
        std::vector<size_t> m_PortIndicesOfAtomPorts;
        logger::Logger m_Logger;
        std::unique_ptr<schedule::Worker> m_ScheduleWorker;
        std::unique_ptr<schedule::Worker> m_StateRestoreWorker;
        UI *m_Ui = nullptr;
        const RealtimeThreadInterface &m_RealtimeThreadInterface;
        bool m_SupportsThreadSafeRestore = false;
    };
    class UI
    {
    public:
        class ContainerWindow : public Gtk::Window
        {
        public:
            ContainerWindow(UI &ui);
            ~ContainerWindow()
            {
            }
            GtkEventBox* Container() const 
            { 
                return Glib::unwrap(m_Container);
            }
            void AttachWidget(GtkWidget* widget)
            {
                Gtk::Widget* gtkmm_widget = Glib::wrap(widget);
                m_Container->add(*gtkmm_widget);
                gtkmm_widget->show_all();
                gtkmm_widget->grab_focus();
                show_all();
            }
            void RemoveWidget()
            {
                m_Container->remove();
            }
        private:
            UI &m_UI;
            Gtk::EventBox *m_Container = nullptr;
        };
        UI(const UI&) = delete;
        UI& operator=(const UI&) = delete;
        UI(UI&&) = delete;
        UI& operator=(UI&&) = delete;
        LV2UI_Request_Value_Status RequestValue(LV2_URID key, LV2_URID type, const LV2_Feature *const *features)
        {
            return LV2UI_REQUEST_VALUE_ERR_UNSUPPORTED;
        }
        UI(Instance &instance);
        ~UI();

        void CallIdle()
        {
            // todo
        }
        utils::NotifySource& OnClose() {return m_OnClose;}
        Instance &instance() {return m_Instance;}
        void OnControlValueChanged(uint32_t portindex, float value)
        {
            if(m_SuilInstance)
            {
                suil_instance_port_event(m_SuilInstance, portindex, 4, 0, &value);
            }
        }
        void OnAtomPortMessage(uint32_t portindex, LV2_URID type, uint32_t datasize, const void *data);
        uint32_t PortIndex(const char *port_symbol) const
        {
            std::string port_symbol_str(port_symbol);
            auto port = m_Instance.plugin().PortBySymbol(port_symbol_str);
            if(port)
            {
                return (uint32_t)port->Index();
            }
            return LV2UI_INVALID_PORT_INDEX; 
        }
        void PortWrite(uint32_t port_index, uint32_t buffer_size, uint32_t protocol, void const *buffer)
        {
            if(port_index < m_Instance.Connections().size())
            {
                auto connection = m_Instance.Connections().at(port_index).get();
                if(protocol == 0)
                {
                    if(auto controlconnection = dynamic_cast<TConnection<TControlPort>*>(connection))
                    {
                        instance().realtimeThreadInterface().SendControlValueFunc(controlconnection, *(float*)buffer);
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
                            }
                        }
                    }
                }
            }

        }

    private:
        Instance &m_Instance;
        std::vector<const LV2_Feature*> m_Features;
        LV2_Feature m_InstanceAccessFeature;
        LV2_Feature m_UiParentFeature;
        LV2_Feature m_DataAccessFeature;
        LV2_Feature m_IdleFeature;
        LV2_Feature m_RequestValueFeature;
        LV2UI_Request_Value m_RequestValue;
        logger::Logger m_Logger;
        const LilvUI* m_Ui = nullptr;
        SuilInstance *m_SuilInstance = nullptr;
        ContainerWindow* m_ContainerWindow = nullptr;
        LV2_URID m_Uridatom_eventTransfer;
        utils::NotifySource m_OnClose;

    };

}
