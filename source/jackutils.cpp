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

    // returns true on success
    bool Port::LinkToPortByName(const std::string &portname)
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
    bool Port::LinkToAnyPortByName(const std::vector<std::string> &portnames)
    {
        for(const auto &portname: portnames)
        {
            if(jack_port_connected_to(m_Port, portname.c_str()))
            {
                return true;
            }
        }
        auto jackclient = Client::Static().get();
        for(const auto &portname: portnames)
        {
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
        }
        return false;
    }
    void Port::LinkToAllPortsByName(const std::vector<std::string> &portnames)
    {
        if(direction() == Direction::Input)
        {
            throw std::runtime_error("LinkToAllPortsByName only works for output ports");
        }
        auto jackclient = Client::Static().get();
        for(const auto &portname: portnames)
        {
            jack_connect(jackclient,  jack_port_name(m_Port), portname.c_str());
        }
    }

}
