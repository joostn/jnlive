#pragma once
#include <string>
#include <memory>
#include <vector>
#include "lilvutils.h"
#include "project.h"
#include "jackutils.h"
#include "realtimethread.h"
#include "engine.h"

namespace engine
{
    class PluginInstance
    {
    public:
        PluginInstance(const PluginInstance&) = delete;
        PluginInstance& operator=(const PluginInstance&) = delete;
        PluginInstance(PluginInstance&&) = delete;
        PluginInstance& operator=(PluginInstance&&) = delete;
        PluginInstance(std::string &&uri, uint32_t samplerate, const std::optional<size_t> &owningPart) : m_Plugin(std::make_unique<lilvutils::Plugin>(lilvutils::Uri(std::move(uri)))), m_Instance(std::make_unique<lilvutils::Instance>(*m_Plugin, samplerate)), m_OwningPart(owningPart)
        {
        }
        const lilvutils::Plugin& Plugin() const { return *m_Plugin; }
        const lilvutils::Instance& Instance() const { return *m_Instance; }
        const std::string& Uri() const { return Plugin().Uri().str(); }
        const std::optional<size_t>& OwningPart() const { return m_OwningPart; }

    private:
        std::unique_ptr<lilvutils::Plugin> m_Plugin;
        std::unique_ptr<lilvutils::Instance> m_Instance;
        std::optional<size_t> m_OwningPart;
    };
    class Engine
    {
    public:
        class Part
        {
        public:
            Part(std::vector<size_t> &&pluginIndices) : m_PluginIndices(std::move(pluginIndices))
            {
            }
            const std::vector<size_t>& PluginIndices() const
            {
                return m_PluginIndices;
            }
            
        private:
            std::vector<size_t> m_PluginIndices;
        };
        Engine()
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
                std::vector<std::unique_ptr<PluginInstance>> pluginsToDiscard;
                auto samplerate = jack_get_sample_rate(jackutils::Client::Static().get());
                std::vector<std::unique_ptr<PluginInstance>> ownedPlugins;
                std::vector<std::vector<size_t>> parts(Project()->Parts().size());
                for)(const auto &instrument : Project()->Instruments())
                {
                    size_t sharedpluginindex = 0;
                    for(size_t partindex = 0; partindex < Project()->Parts().size(); ++partindex)
                    {
                        auto &part = parts[partindex];
                        std::optional<size_t> owningpart;
                        if(!instrument.Shared())
                        {
                            owningpart = partindex;
                        }
                        if(instrument.Shared() && (partindex > 0))
                        {
                            part.push_back(sharedpluginindex);
                        }
                        else
                        {
                            auto pluginindex = ownedPlugins.size();
                            sharedpluginindex = pluginindex;
                            auto plugin_uq = std::make_unique<PluginInstance>(instrument.Lv2Uri(), instrument.Lv2Uri(), samplerate, owningpart);
                            part.push_back(pluginindex);
                            ownedPlugins.push_back(std::move(plugin_uq));
                        }
                    }
                }
                std::vector<Part> m_Parts;
                for(size_t partindex = 0; partindex < Project()->Parts().size(); ++partindex)
                {
                    std::vector<size_t> pluginindices;
                    for(size_t instrumentindex = 0; instrumentindex < Project()->Instruments().size(); ++instrumentindex)
                    {
                        pluginindices.push_back(parts[partindex][instrumentindex]);
                    }
                    m_Parts.push_back(Part(std::move(pluginindices)));
                }
                pluginsToDiscard = std::move(m_OwnedPlugins);
                m_OwnedPlugins = std::move(ownedPlugins);
                m_Parts = std::move(m_Parts);
                //todo: pluginsToDiscard
            }
            
        }
        realtimethread::Data CalcRtData() const
        {
            if(m_OwnedPlugins.size() > realtimethread::kMaxNumPlugins)
            {
                throw std::runtime_error("m_OwnedPlugins.size() > realtimethread::kMaxNumPlugins");
            }
            if(m_Parts.size() > realtimethread::kMaxNumMidiPorts)
            {
                throw std::runtime_error("m_Parts.size() > realtimethread::kMaxNumMidiPorts");
            }
            realtimethread::Data result;
            float maxamplitude = 0;
            for(const auto &part: Project()->Parts())
            {
                maxamplitude = std::max(maxamplitude, part.AmplitudeFactor());
            }
            for(const auto &ownedplugin: m_OwnedPlugins)
            {
                float amplitude = 0;
                if(ownedplugin->OwningPart())
                {
                    amplitude = Project()->Parts()[*ownedplugin->OwningPart()].AmplitudeFactor();
                }
                else
                {
                    amplitude = maxamplitude;
                }
                realtimethread::Data::Plugin plugin(ownedplugin.get(), amplitude);
                result.xxx
            }

        }
        const std::vector<PluginInstance>& OwnedPlugins() const { return m_OwnedPlugins; }
        const std::vector<Part>& Parts() const { return m_Parts; }
    private:
        std::shared_ptr<project::Project> m_Project = std::make_shared<project::Project>();
        std::vector<std::unique_ptr<PluginInstance>> m_OwnedPlugins;
        std::vector<Part> m_Parts;
        realtimethread::Processor m_RtProcessor(8192);
        lilvutils::World m_LilvWorld;
        jackutils::Client m_JackClient("JN Live", [&m_RtProcessor](jack_nframes_t nframes){
            m_RtProcessor.Process(nframes);
        });
        jackutils::Port m_MidiInPort("midi_in", jackutils::Port::Kind::Midi, jackutils::Port::Direction::In);
        std::array<jackutils::Port, 2> m_AudioOutPorts = { jackutils::Port("out_l", jackutils::Port::Kind::Audio, jackutils::Port::Direction::Out), jackutils::Port("out_r", jackutils::Port::Kind::Audio, jackutils::Port::Direction::Out) };

    };
}