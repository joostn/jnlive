#pragma once
#include <stdexcept>	
#include <jack/jack.h>
#include <string>
#include <atomic>
#include "utils.h"

namespace jackutils
{
    class ProcessBlocker;
    class Client
    {
        friend ProcessBlocker;
    public:
        Client(const Client&) = delete;
        Client& operator=(const Client&) = delete;
        Client(Client&&) = delete;
        Client& operator=(Client&&) = delete;
        Client(const std::string &name)
        {
            if(staticptr())
            {
                throw std::runtime_error("Can only instantiate a single jackutils::Client");
            }
            auto jackclient = jack_client_open(name.c_str(), JackNullOption, nullptr);
            if (!jackclient) {
                throw std::runtime_error("Failed to open JACK client.");
            }
            utils::finally fin1([&](){
                if(jackclient)
                {
                    jack_client_close(jackclient); 
                }
            });
            jack_set_process_callback(jackclient, &Client::processStatic, this);
            m_Client = jackclient;
            jackclient = nullptr;
            staticptr() = this;
        }
        ~Client()
        {
            if(m_Client) jack_client_close(m_Client);
            staticptr() = nullptr;
        }
        jack_client_t* get()
        {
            return m_Client;
        }
        static int processStatic(jack_nframes_t nframes, void* arg);
        static int processStaticDummy(jack_nframes_t nframes, void* arg);
        int process(jack_nframes_t nframes);
        static Client& Static()
        {
            if(!staticptr())
            {
                throw std::runtime_error("Client not instantiated");
            }
            return *staticptr();
        }
        
    private:
        static Client*& staticptr()
        {
            static Client *s_Client = nullptr;
            return s_Client;
        }
        void BlockProcess();
        void UnblockProcess();

    private:
        jack_client_t *m_Client = nullptr;
        std::atomic<bool> m_Processing{false};
        int m_ProcessBlockCount = 0;
    };
    class ProcessBlocker
    {
    public:
        ProcessBlocker(ProcessBlocker&&) = delete;
        ProcessBlocker& operator=(ProcessBlocker&&) = delete;
        ProcessBlocker(const ProcessBlocker&) = delete;
        ProcessBlocker& operator=(const ProcessBlocker&) = delete;
        ProcessBlocker()
        {
            Client::Static().BlockProcess();
        }
        ~ProcessBlocker()
        {
            Client::Static().UnblockProcess();
        }
    };
    class Port
    {
    public:
        enum class Kind {Audio, Midi};
        enum class Direction {Input, Output};
        Port(const Port&) = delete;
        Port& operator=(const Port&) = delete;
        Port(Port&&) = delete;
        Port& operator=(Port&&) = delete;
        Port(std::string &&name, Kind kind, Direction direction) : m_Kind(kind), m_Direction(direction), m_Name(name)
        {
            auto jackclient = Client::Static().get();
            auto jackport = jack_port_register(jackclient, m_Name.c_str(), 
                kind == Kind::Audio ? JACK_DEFAULT_AUDIO_TYPE : JACK_DEFAULT_MIDI_TYPE, 
                direction == Direction::Input ? JackPortIsInput : JackPortIsOutput, 0);
            if (!jackport) {
                throw std::runtime_error("Failed to open JACK port.");
            }
            utils::finally fin1([&](){
                if(jackport)
                {
                    jack_port_unregister(Client::Static().get(), jackport); 
                }
            });
            m_Port = jackport;
            jackport = nullptr;
        }
        ~Port()
        {
            if(m_Port) jack_port_unregister(Client::Static().get(), m_Port);
        }
        Kind kind() const { return m_Kind; }
        Direction direction() const { return m_Direction; }
        jack_port_t* get()
        {
            return m_Port;
        }
        const jack_port_t* get() const
        {
            return m_Port;
        }
        const std::string& name() const
        {
            return m_Name;
        }

    private:
        jack_port_t *m_Port = nullptr;
        Kind m_Kind;
        std::string m_Name;
        Direction m_Direction;
    };
}