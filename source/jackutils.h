#pragma once
#include <stdexcept>	
#include <jack/jack.h>
#include <string>
#include "utils.h"

namespace jackutils
{
    class Client
    {
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
            jack_set_process_callback(jackclient, [](jack_nframes_t nframes, void* arg){
                auto theclient = (Client*)arg;
                if (!theclient) {
                    throw std::runtime_error("Failed to open JACK client.");
                }
                return theclient->process(nframes);
            }, this);
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
        int process(jack_nframes_t nframes)
        {
            return 0;
        }
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

    private:
        jack_client_t *m_Client = nullptr;
    };
}