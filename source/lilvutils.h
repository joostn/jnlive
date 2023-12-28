#pragma once
#include "utils.h"
#include <lilv/lilv.h>

namespace lilvutils
{
    class world
    {
    public:
        world(const world&) = delete;
        world& operator=(const world&) = delete;
        world(world&&) = delete;
        world& operator=(world&&) = delete;
        world()
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
        ~world()
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
        static world& Static()
        {
            if(!staticptr())
            {
                throw std::runtime_error("lilv_world not instantiated");
            }
            return *staticptr();
        }
        
    private:
        static world*& staticptr()
        {
            static world *s_World = nullptr;
            return s_World;
        }
    private:
        LilvWorld* m_World = nullptr;
        const LilvPlugins *m_Plugins = nullptr;
    };
    class uri
    {
    public:
        uri(const std::string &name)
        {
            m_node = lilv_new_uri(world::Static().get(), name.c_str());
            if(!m_node)
            {
                throw std::runtime_error("lilv_new_uri ("+name+") failed");
            }
        }
        ~uri()
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
    class plugin
    {
    public:
        plugin(const uri &uri)
        {
            auto plugins = world::Static().Plugins();
            auto urinode = uri.get();
            m_Plugin = lilv_plugins_get_by_uri(plugins, urinode); // return value must not be freed
            if(!m_Plugin)
            {
                throw std::runtime_error("could not load plugin");
            }
        }
        ~plugin()
        {
            // no need to free
        }
        const LilvPlugin* get() const { return m_Plugin; }
    private:
        const LilvPlugin *m_Plugin = nullptr;
    };
    class instance
    {
    public:
        instance(const plugin &plugin, double sample_rate, LV2_Feature** features)
        {
            m_Instance = lilv_plugin_instantiate(plugin.get(), sample_rate, features);
            if(!m_Instance)
            {
                throw std::runtime_error("could not instantiate plugin");
            }
        }
        ~instance()
        {
            if(m_Instance)
            {
                lilv_instance_free(m_Instance);
            }
        }
        const LilvInstance* get() const { return m_Instance; }
    private:
        LilvInstance *m_Instance = nullptr;
    };
}
