#pragma once
#include <stdexcept>
#include <memory>	
#include <optional>
#include "lilvutils.h"
#include "jackutils.h"

namespace lilvjacklink {
    class LinkedPluginInstance;
    class PortLink
    {
    public:
        PortLink(const PortLink&) = delete;
        PortLink& operator=(const PortLink&) = delete;
        PortLink(PortLink&&) = delete;
        PortLink& operator=(PortLink&&) = delete;
        PortLink(std::unique_ptr<jackutils::Port> &&jackport, lilvutils::Instance &instance, uint32_t portindex);
        ~PortLink();
        int process1(jack_nframes_t nframes);
        int process2(jack_nframes_t nframes);
    private:
        std::unique_ptr<jackutils::Port> m_JackPort;
        lilvutils::Instance &m_Instance;
        LV2_Evbuf* m_Evbuf = nullptr;
        uint32_t m_PortIndex;
        LV2_URID m_UridMidiEvent;
    };
    class Global
    {
        friend PortLink;
        friend LinkedPluginInstance;
    public:
        Global(const Global&) = delete;
        Global& operator=(const Global&) = delete;
        Global(Global&&) = delete;
        Global& operator=(Global&&) = delete;
        Global()
        {
            if(staticptr())
            {
                throw std::runtime_error("Can only instantiate a single lilvjacklink::Global");
            }
            staticptr() = this;
        }
        ~Global()
        {
            staticptr() = nullptr;
        }
        int process(jack_nframes_t nframes);
        static Global& Static()
        {
            if(!staticptr())
            {
                throw std::runtime_error("Global not instantiated");
            }
            return *staticptr();
        }
        static Global* StaticPtr()
        {
            return staticptr();
        }
        
    private:
        static Global*& staticptr()
        {
            static Global *s_Global = nullptr;
            return s_Global;
        }

    private:
        std::vector<PortLink*> m_PortLinks;
        std::vector<LinkedPluginInstance*> m_InstanceLinks;
    };

    class LinkedPluginInstance
    {
    public:
        LinkedPluginInstance(LinkedPluginInstance&&) = delete;
        LinkedPluginInstance& operator=(LinkedPluginInstance&&) = delete;
        LinkedPluginInstance(const LinkedPluginInstance&) = delete;
        LinkedPluginInstance& operator=(const LinkedPluginInstance&) = delete;
        LinkedPluginInstance(lilvutils::Instance &instance, const std::string &instancename);
        ~LinkedPluginInstance();
        int process(jack_nframes_t nframes);

    private:
        lilvutils::Instance &m_Instance;
        std::vector<std::unique_ptr<PortLink>> m_PortLinks;
    };



}