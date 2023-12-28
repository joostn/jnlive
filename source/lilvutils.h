#pragma once

#include <stdexcept>	
#include "utils.h"
#include <lilv/lilv.h>
#include "lv2/atom/atom.h"

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
        
    private:
        static World*& staticptr()
        {
            static World *s_World = nullptr;
            return s_World;
        }

    private:
        LilvWorld* m_World = nullptr;
        const LilvPlugins *m_Plugins = nullptr;
    };
    class Uri
    {
    public:
        Uri(const Uri&) = delete;
        Uri& operator=(const Uri&) = delete;
        Uri(Uri&&) = delete;
        Uri& operator=(Uri&&) = delete;
        Uri(const std::string &name)
        {
            m_node = lilv_new_uri(World::Static().get(), name.c_str());
            if(!m_node)
            {
                throw std::runtime_error("lilv_new_uri ("+name+") failed");
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
    private:
        LilvNode *m_node = nullptr;
    };
    class Plugin
    {
    public:
        Plugin(const Plugin&) = delete;
        Plugin& operator=(const Plugin&) = delete;
        Plugin(Plugin&&) = delete;
        Plugin& operator=(Plugin&&) = delete;
        Plugin(const Uri &uri)
        {
            auto lilvworld = World::Static().get();
            auto plugins = World::Static().Plugins();
            auto urinode = uri.get();
            m_Plugin = lilv_plugins_get_by_uri(plugins, urinode); // return value must not be freed
            if(!m_Plugin)
            {
                throw std::runtime_error("could not load plugin");
            }
            lilvutils::Uri uri_InputPort(LV2_CORE__InputPort);
            lilvutils::Uri uri_control(LV2_CORE__control);
            lilvutils::Uri uri_AtomPort(LV2_ATOM__AtomPort);
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
        ~Plugin()
        {
            // no need to free
        }
        const LilvPlugin* get() const { return m_Plugin; }
    private:
        const LilvPlugin *m_Plugin = nullptr;
        std::optional<uint32_t> m_ControlInputIndex;
    };
    class Instance
    {
    public:
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&&) = delete;
        Instance& operator=(Instance&&) = delete;
        Instance(const Plugin &plugin, double sample_rate, LV2_Feature** features) : m_Plugin(plugin)
        {
            m_Instance = lilv_plugin_instantiate(plugin.get(), sample_rate, features);
            if(!m_Instance)
            {
                throw std::runtime_error("could not instantiate plugin");
            }

        }
        ~Instance()
        {
            if(m_Instance)
            {
                lilv_instance_free(m_Instance);
            }
        }
        const LilvInstance* get() const { return m_Instance; }
        LilvInstance* get() { return m_Instance; }
        const Plugin& plugin() const { return m_Plugin; }
    private:
        LilvInstance *m_Instance = nullptr;
        const Plugin &m_Plugin;
    };
}
