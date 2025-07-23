#include "jackutils.h"
#include <iostream>

namespace jackutils
{
    Port::Port(std::string &&name, PortKind kind, PortDirection direction) : m_Kind(kind), m_Direction(direction), m_Name(name)
    {
        auto jackclient = Client::Static().get();
        auto jackport = jack_port_register(jackclient, m_Name.c_str(), 
            kind == PortKind::Audio ? JACK_DEFAULT_AUDIO_TYPE : JACK_DEFAULT_MIDI_TYPE, 
            direction == PortDirection::Input ? JackPortIsInput : JackPortIsOutput, 0);
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
        // std::cout << "Port " << m_Name << " created\n";
    }
    Port::~Port()
    {
        if(m_Port) jack_port_unregister(Client::Static().get(), m_Port);
    }
    
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

        jack_set_port_registration_callback(jackclient, [] (jack_port_id_t port, int reg, void *arg){
            // std::cout << "port registration: "  << reg << "\n";
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
        // ListAllPorts();
        // https://jackaudio.org/api/group__PortFunctions.html#gae6090e81f2ee23b5c0e432a899085ec8

    }

    void Client::ListAllPorts()
    {
        auto ports = jack_get_ports(m_Client, nullptr, nullptr, 0);
        utils::finally fin1([&](){
            if(ports)
            {
                jack_free(ports);
            }
        });
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
    }

    std::vector<std::string> Client::GetAllPorts(jackutils::PortKind kind, jackutils::PortDirection direction) const
    {
        std::vector<std::string> result;
        auto ports = jack_get_ports(m_Client, nullptr, kind == jackutils::PortKind::Midi? "midi":"audio", direction == jackutils::PortDirection::Input ? JackPortIsInput : JackPortIsOutput);
        utils::finally fin1([&](){
            if(ports)
            {
                jack_free(ports);
            }
        });
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
                result.push_back(port);
            }
        }
        return result;
    }

    // returns true on success
    bool Port::LinkToPortByName(const std::string &portname)
    {
        auto jackclient = Client::Static().get();
        int result;
        if(direction() == PortDirection::Input)
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
    std::vector<std::string> Port::GetMatchingPortNames(const std::vector<std::regex> &portnames)
    {
        auto jackclient = Client::Static().get();
        std::vector<std::string> matchingPortNames;
        auto ports = jack_get_ports(jackclient, nullptr, nullptr, 0);
        if(ports)
        {
            utils::finally fin1([&](){
                jack_free(ports);
            });
            size_t index = 0;
            while(true)
            {
                const char *port = ports[index++];
                if(!port) break;
                std::string port_str(port);
                for(const auto &portname: portnames)
                {
                    if(std::regex_match(port, portname))
                    {
                        matchingPortNames.push_back(port);
                    }
                }
            }
        }
        return matchingPortNames;
    }
    bool Port::LinkToAnyPortByPattern(const std::vector<std::string> &portnames)
    {
        std::vector<std::regex> patterns;
        for(const auto &portname: portnames)
        {
            patterns.push_back(utils::makeSimpleRegex(portname));
        }
        return LinkToAnyPortByName(patterns);
    }

    bool Port::LinkToAnyPortByName(const std::vector<std::regex> &portnames)
    {
        auto jackclient = Client::Static().get();
        auto matchingportnames = GetMatchingPortNames(portnames);
        for(const auto &portname: matchingportnames)
        {
            if(jack_port_connected_to(m_Port, portname.c_str()))
            {
                return true;
            }
        }
        for(const auto &portname: matchingportnames)
        {
            int result;
            if(direction() == PortDirection::Input)
            {
                result = jack_connect(jackclient, portname.c_str(), jack_port_name(m_Port));
            }
            else
            {
                result = jack_connect(jackclient, jack_port_name(m_Port), portname.c_str());
            }
            if(result == 0)
            {
                // std::cout << "- Connected to " << portname << "\n";
            }
            if( (result == 0) || (result == EEXIST) )
            {
                return true;
            }
        }
        return false;
    }
    void Port::LinkToAllPortsByPattern(const std::vector<std::string> &portnames)
    {
        std::vector<std::regex> patterns;
        for(const auto &portname: portnames)
        {
            patterns.push_back(utils::makeSimpleRegex(portname));
        }
        LinkToAllPortsByName(patterns);
    }
    void Port::LinkToAllPortsByName(const std::vector<std::regex> &portnames)
    {
        auto jackclient = Client::Static().get();
        auto matchingportnames = GetMatchingPortNames(portnames);
        if(direction() == PortDirection::Input)
        {
            throw std::runtime_error("LinkToAllPortsByName only works for output ports");
        }
        for(const auto &portname: matchingportnames)
        {
            jack_connect(jackclient,  jack_port_name(m_Port), portname.c_str());
        }
    }

}
