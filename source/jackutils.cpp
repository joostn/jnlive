#include "jackutils.h"
#include <iostream>

namespace jackutils
{
    Client::Client(const std::string &name, std::function<void(jack_nframes_t nframes)> &&processcallback) : m_ProcessCallback(std::move(processcallback))
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
        jack_set_xrun_callback 	(jackclient, [](void*) -> int {
            std::cout << "XRUN!\n";
            return 0;
        }, this);

        // jack_on_shutdown(jackclient, [](void* arg){
        //     auto theclient = (Client*)arg;
        //     if (theclient)
        //     {
        //         theclient->OnShutdown();
        //     }
        // }, this);
        m_Client = jackclient;
        jackclient = nullptr;
        staticptr() = this;
        ListAllPorts();

        // https://jackaudio.org/api/group__PortFunctions.html#gae6090e81f2ee23b5c0e432a899085ec8

    }
}
