#pragma once

#include <stdexcept>	
#include "utils.h"
#include "schedule.h"
#include <lilv/lilv.h>
#include <mutex>
#include <optional>
#include <map>
#include "lv2/atom/atom.h"
#include "lv2_evbuf.h"
#include "log.h"
#include "lv2/log/log.h"
#include "lv2/options/options.h"
#include "lv2/parameters/parameters.h"
#include "lv2/ui/ui.h"
#include "lv2/buf-size/buf-size.h"

namespace lilvutils
{
    class World
    {
    public:
        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = delete;
        World& operator=(World&&) = delete;
        World(uint32_t sample_rate, uint32_t maxBlockSize);
        ~World()
        {
            if(m_World) lilv_world_free(m_World);
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
        const LilvPlugins *m_Plugins = nullptr;
        LV2_URID_Map      m_UridMap;
        LV2_URID_Unmap    m_UridUnmap;
        LV2_Feature m_MapFeature;
        LV2_Feature m_UnmapFeature;
        LV2_Feature m_OptionsFeature;
        std::vector<const LV2_Feature*> m_Features;
        std::vector<LV2_Options_Option> m_Options;
        float m_OptionSampleRate = 0.0f;
        uint32_t m_OptionMinBlockLength = 16;
        uint32_t m_OptionMaxBlockLength = 4096;
        uint32_t m_OptionSequenceSize = 4096;
        float m_OptionUiUpdateRate = 30.0f;
        float m_OptionUiScaleFactor = 1.0f;
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
        TPortBase(TDirection direction, std::string &&name, std::string &&symbol, bool optional) : m_Direction(direction), m_Name(std::move(name)), m_Symbol(std::move(symbol)), m_Optional(optional) {}
        virtual ~TPortBase() {}
        TDirection Direction() const { return m_Direction; }
        const std::string& Name() const { return m_Name; }
        const std::string& Symbol() const { return m_Symbol; }
        bool Optional() const { return m_Optional; }
    private:
        TDirection m_Direction;
        std::string m_Name;
        std::string m_Symbol;
        bool m_Optional;
    };
    class TAudioPort : public TPortBase
    {
    public:
        enum class TDesignation {StereoLeft, StereoRight};
        TAudioPort(TDirection direction, std::string &&name, std::string &&symbol, std::optional<TDesignation> designation, bool optional) : TPortBase(direction, std::move(name), std::move(symbol), optional), m_Designation(designation) {}
        virtual ~TAudioPort() {}
        std::optional<TDesignation> Designation() const { return m_Designation; }
    private:
        std::optional<TDesignation> m_Designation;
    };
    class TCvPort : public TPortBase
    {
    public:
        TCvPort(TDirection direction, std::string &&name, std::string &&symbol, bool optional) : TPortBase(direction, std::move(name), std::move(symbol), optional) {}
        virtual ~TCvPort() {}
    };
    class TAtomPort : public TPortBase
    {
    public:
        enum class TDesignation {Control};
        TAtomPort(TDirection direction, std::string &&name, std::string &&symbol, std::optional<TDesignation> designation, bool supportsmidi, const std::optional<int> &minbuffersize, bool optional) : TPortBase(direction, std::move(name), std::move(symbol), optional), m_Designation(designation), m_SupportsMidi(supportsmidi), m_MinBufferSize(minbuffersize) {}
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
        TControlPort(TDirection direction, std::string &&name, std::string &&symbol, float minimum, float maximum, float defaultvalue, bool optional) : TPortBase(direction, std::move(name), std::move(symbol), optional), m_Minimum(minimum), m_Maximum(maximum), m_DefaultValue(defaultvalue) {}
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

    private:
        std::string m_Uri;
        std::string m_Name;
        const LilvPlugin *m_Plugin = nullptr;
        std::optional<uint32_t> m_MidiInputIndex;
        std::vector<std::unique_ptr<TPortBase>> m_Ports;
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
        TConnection(size_t bufsize) : m_Buffer(bufsize)
        {
            std::fill(m_Buffer.begin(), m_Buffer.end(), 0.0f);
        }
        virtual ~TConnection() {}
        float *Buffer() { return &m_Buffer[0]; }
    private:
        std::vector<float> m_Buffer;
    };
    template <>
    class TConnection<TCvPort> : public TConnectionBase
    {
    public:
        TConnection(size_t bufsize) : m_Buffer(bufsize) 
        {
            std::fill(m_Buffer.begin(), m_Buffer.end(), 0.0f);
        }
        virtual ~TConnection() {}
        float *Buffer() { return &m_Buffer[0]; }
    private:
        std::vector<float> m_Buffer;
    };
    template <>
    class TConnection<TAtomPort> : public TConnectionBase
    {
    public:
        TConnection(uint32_t bufsize)
        {
            auto urid_chunk = lilvutils::World::Static().UriMapLookup(LV2_ATOM__Chunk);
            auto urid_sequence = lilvutils::World::Static().UriMapLookup(LV2_ATOM__Sequence);
            m_EvBuf = lv2_evbuf_new(bufsize, urid_chunk, urid_sequence);
        }
        virtual ~TConnection()
        {
            if(m_EvBuf)
            {
                lv2_evbuf_free(m_EvBuf);
            }
        }
        LV2_Evbuf* Buffer() const { return m_EvBuf; }
    private:
        LV2_Evbuf* m_EvBuf = nullptr;
    };
    template <>
    class TConnection<TControlPort> : public TConnectionBase
    {
    public:
        TConnection(const float &defaultvalue) : m_Value(defaultvalue)
        {
        }
        virtual ~TConnection() {}
        float* Buffer() { return &m_Value; }
    private:
        float m_Value;
    };
    class Instance
    {
    public:
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&&) = delete;
        Instance& operator=(Instance&&) = delete;
        Instance(const Plugin &plugin, double sample_rate);
        ~Instance();

        void Run(uint32_t samplecount)
        {
            lilv_instance_run(m_Instance, samplecount);
            if(m_ScheduleWorker)
            {
                m_ScheduleWorker->RunInRealtimeThread();
            }
            for(auto index: m_PortIndicesOfAtomPorts)
            {
                bool isinput = m_Plugin.Ports().at(index)->Direction() == TPortBase::TDirection::Input;
                if(auto atomconnection = dynamic_cast<TConnection<TAtomPort>*>(m_Connections.at(index).get()))
                {
                    lv2_evbuf_reset(atomconnection->Buffer(), isinput);
                }
            }
        }
        const LilvInstance* get() const { return m_Instance; }
        LV2_Handle Handle() const { return lilv_instance_get_handle(m_Instance); }
        LilvInstance* get() { return m_Instance; }
        const Plugin& plugin() const { return m_Plugin; }
        const std::vector<std::unique_ptr<TConnectionBase>>& Connections() const { return m_Connections; }
        LV2_Evbuf* MidiInEvBuf() const;
    private:
        const Plugin &m_Plugin;
        std::vector<const LV2_Feature*> m_Features;
        LilvInstance *m_Instance = nullptr;
        std::vector<std::unique_ptr<TConnectionBase>> m_Connections;
        std::vector<size_t> m_PortIndicesOfAtomPorts;
        logger::Logger m_Logger;
        std::unique_ptr<schedule::Worker> m_ScheduleWorker;
    };
}
