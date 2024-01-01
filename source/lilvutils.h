#pragma once

#include <stdexcept>	
#include "utils.h"
#include <lilv/lilv.h>
#include <mutex>
#include <optional>
#include <map>
#include "lv2/atom/atom.h"
#include "lv2_evbuf.h"

namespace lilvutils
{
    class World
    {
    public:
        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = delete;
        World& operator=(World&&) = delete;
        World()
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
            m_Features.push_back(nullptr);
        }
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
        const LV2_Feature** GetFeatures()
        {
            return m_Features.data();
        }

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
        std::vector<const LV2_Feature*> m_Features;
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
        const std::optional<uint32_t>& ControlInputIndex() const { return m_ControlInputIndex; }
        const std::array<std::optional<uint32_t>,2>& OutputAudioPortIndices() const { return m_AudioOutputIndex; }
        const std::string& Lv2Uri() const { return m_Uri; }
    private:
        std::string m_Uri;
        const LilvPlugin *m_Plugin = nullptr;
        std::optional<uint32_t> m_ControlInputIndex;
        std::array<std::optional<uint32_t>,2> m_AudioOutputIndex;
    };
    class Instance
    {
    public:
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&&) = delete;
        Instance& operator=(Instance&&) = delete;
        Instance(const Plugin &plugin, double sample_rate) : m_Plugin(plugin)
        {
            if(plugin.ControlInputIndex())
            {
                auto urid_chunk = lilvutils::World::Static().UriMapLookup(LV2_ATOM__Chunk);
                auto urid_sequence = lilvutils::World::Static().UriMapLookup(LV2_ATOM__Sequence);
                uint32_t bufsize = 100000; // bufsizeOrDefault.value_or(100000);
                m_MidiInEvBuf = lv2_evbuf_new(bufsize, urid_chunk, urid_sequence);
                if(!m_MidiInEvBuf)
                {
                    throw std::runtime_error("could not create evbuf");
                }
                lv2_evbuf_reset(m_MidiInEvBuf, true);
            }
            m_Instance = lilv_plugin_instantiate(plugin.get(), sample_rate, World::Static().GetFeatures());
            if(!m_Instance)
            {
                throw std::runtime_error("could not instantiate plugin");
            }
            if(plugin.ControlInputIndex())
            {
                lilv_instance_connect_port(m_Instance, plugin.ControlInputIndex().value(), lv2_evbuf_get_buffer(m_MidiInEvBuf));
            }

        }
        ~Instance()
        {
            if(m_Instance)
            {
                lilv_instance_free(m_Instance);
            }
            if(m_MidiInEvBuf)
            {
                lv2_evbuf_free(m_MidiInEvBuf);
            }
        }
        const LilvInstance* get() const { return m_Instance; }
        LilvInstance* get() { return m_Instance; }
        const Plugin& plugin() const { return m_Plugin; }
        LV2_Evbuf* MidiInEvBuf() const { return m_MidiInEvBuf; }
    private:
        LilvInstance *m_Instance = nullptr;
        LV2_Evbuf* m_MidiInEvBuf = nullptr;
        const Plugin &m_Plugin;
    };
}
