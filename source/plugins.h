#pragma once
#include <string>
#include <memory>
#include <vector>
#include "lilvutils.h"
#include "lilvjacklink.h"

namespace plugins
{
    class Plugin
    {
    public:
        Plugin(const Plugin&) = delete;
        Plugin& operator=(const Plugin&) = delete;
        Plugin(Plugin&&) = delete;
        Plugin& operator=(Plugin&&) = delete;
        Plugin(std::string &&uri, uint32_t samplerate) : m_Plugin(std::make_unique<lilvutils::Plugin>(lilvutils::Uri(std::move(uri)))), m_Instance(std::make_unique<lilvutils::Instance>(*m_Plugin, samplerate)), m_LinkedPluginInstance(std::make_unique<lilvjacklink::LinkedPluginInstance>(*m_Instance))
        {
        }
        const lilvutils::Plugin& Plugin() const { return *m_Plugin; }
        const lilvutils::Instance& Instance() const { return *m_Instance; }
        const lilvjacklink::LinkedPluginInstance& LinkedPluginInstance() const { return *m_LinkedPluginInstance; }
        const std::string& Uri() const { return Plugin().Uri().str(); }

    private:
        std::unique_ptr<lilvutils::Plugin> m_Plugin;
        std::unique_ptr<lilvutils::Instance> m_Instance;
        std::make_unique<lilvjacklink::LinkedPluginInstance> m_LinkedPluginInstance;
    };
    class Plugins
    {
    public:
        class Part
        {
        public:
            Part(std::vector<Plugin*> &&plugins) : m_Plugins(std::move(plugins))
            {
            }
            const std::vector<Plugin*>& Plugins() const
            {
                return m_Plugins;
            }
            
        private:
            std::vector<Plugin*> m_Plugins;
        };
        Plugins()
        {
        }
        const std::shared_ptr<project::Project>& Project() const { return m_Project; }
        void SetProject(const std::shared_ptr<project::Project> &project)
        {
            if(!project)
            {
                throw std::runtime_error("project is null");
            }
            if(m_Project == project) return;
            SyncPlugins();
        }
        bool PluginsChanged() const
        {
            if(Parts().size() != Project()->Parts().size()) return true;
            for(const auto &part : Parts())
            {
                if(part.Plugins().size() != Project()->Instruments().size()) return true;
                for(size_t i = 0; i < part.Plugins().size(); ++i)
                {
                    if(part.Plugins()[i]->Uri() != Project()->Instruments()[i].Lv2Uri()) return true;
                }
            }
            return false;
        }
        void SyncPlugins()
        {
            if(PluginsChanged())
            {
                // todo: re-use plugins
                auto samplerate = jack_get_sample_rate(jackutils::Client::Static().get());
                std::vector<std::unique_ptr<Plugin>> ownedPlugins;
                std::vector<std::vector<Plugin*>> parts(Parts().size());
                for)(const auto &instrument : Project()->Instruments())
                {
                    ownedPlugins.push_back(std::make_unique<Plugin>(instrument.Lv2Uri(), instrument.Lv2Uri(), samplerate));
                }


            }
            
        }
        const std::vector<Plugin>& OwnedPlugins() const { return m_OwnedPlugins; }
        void SetOwnedPlugins(std::vector<Plugin>&& ownedPlugins) { m_OwnedPlugins = std::move(ownedPlugins); }
        const std::vector<Part>& Parts() const { return m_Parts; }
        void SetParts(std::vector<Part>&& parts) { m_Parts = std::move(parts); }
    private:
        std::shared_ptr<project::Project> m_Project = std::make_shared<project::Project>();
        std::vector<std::unique_ptr<Plugin>> m_OwnedPlugins;
        std::vector<Part> m_Parts;
    };
}