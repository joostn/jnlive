#pragma once
#include <stdexcept>	
#include <jack/jack.h>
#include <string>
#include <atomic>
#include "utils.h"
#include <mutex>
#include <condition_variable>

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
        Client(const std::string &name, std::function<void(jack_nframes_t nframes)> &&processcallback);
        // void OnShutdown()
        // {
        //     std::unique_lock<std::mutex> lock(m_Mutex);
        //     m_HasShutdown = true;
        //     m_Condition.notify_all();
        // }
        void ListAllPorts()
        {
            auto ports = jack_get_ports(m_Client, nullptr, nullptr, 0);
            if(ports)
            {
                size_t index = 0;
                while(true)
                {
                    const char *port = ports[index++];
                    if(!port)
                    {
                        break;
                    }
                    printf("%s\n", port);
                }
            }
            jack_free(ports);
        }
        void ShutDown()
        {
            if(m_Client) 
            {
                jack_deactivate(m_Client);
                if(m_Client) jack_client_close(m_Client);
                m_Client= nullptr;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        ~Client()
        {
            ShutDown();
            staticptr() = nullptr;
        }
        jack_client_t* get()
        {
            return m_Client;
        }
        static Client& Static()
        {
            if(!staticptr())
            {
                throw std::runtime_error("Client not instantiated");
            }
            return *staticptr();
        }
        uint32_t SampleRate() const
        {
            return jack_get_sample_rate(m_Client);
        }
        uint32_t BufferSize() const
        {
            return jack_get_buffer_size(m_Client);
        }
    private:
        static Client*& staticptr()
        {
            static Client *s_Client = nullptr;
            return s_Client;
        }
        static int processStatic(jack_nframes_t nframes, void* arg)
        {
            auto theclient = (Client*)arg;
            if (theclient)
            {
                return theclient->process(nframes);
            }
            return 0;
        }
        int process(jack_nframes_t nframes)
        {
            m_ProcessCallback(nframes);
            return 0;
        }

    private:
        jack_client_t *m_Client = nullptr;
        std::function<void(jack_nframes_t nframes)> m_ProcessCallback;
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
        jack_port_t* get() const
        {
            return m_Port;
        }
        const std::string& name() const
        {
            return m_Name;
        }
        // returns true on success
        bool LinkToPortByName(const std::string &portname)
        {
            auto jackclient = Client::Static().get();
            int result;
            if(direction() == Direction::Input)
            {
                result = jack_connect(jackclient, portname.c_str(), jack_port_name(m_Port));
            }
            else
            {
                result = jack_connect(jackclient, jack_port_name(m_Port), portname.c_str());
            }
            if( (result == 0) || (result == EEXIST) )
            {
                return true;
            }
            return false;
        }

    private:
        jack_port_t *m_Port = nullptr;
        Kind m_Kind;
        std::string m_Name;
        Direction m_Direction;
    };
}