#pragma once
#include <stdexcept>	
#include <jack/jack.h>
#include <string>
#include <atomic>
#include "utils.h"
#include <mutex>
#include <condition_variable>
#include <regex>

namespace jackutils
{
    enum class PortKind {Audio, Midi};
    enum class PortDirection {Input, Output};

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
        void ListAllPorts();
        std::vector<std::string> GetAllPorts(jackutils::PortKind kind, jackutils::PortDirection) const;
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
        Port(const Port&) = delete;
        Port& operator=(const Port&) = delete;
        Port(Port&&) = delete;
        Port& operator=(Port&&) = delete;
        Port(std::string &&name, PortKind kind, PortDirection direction);
        ~Port();
        PortKind kind() const { return m_Kind; }
        PortDirection direction() const { return m_Direction; }
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
        bool LinkToPortByName(const std::string &portname);
        bool LinkToAnyPortByPattern(const std::vector<std::string> &portnames);
        bool LinkToAnyPortByName(const std::vector<std::regex> &portnames);
        void LinkToAllPortsByPattern(const std::vector<std::string> &portnames);
        void LinkToAllPortsByName(const std::vector<std::regex> &portnames);
        std::vector<std::string> GetMatchingPortNames(const std::vector<std::regex> &portnames);

    private:
        jack_port_t *m_Port = nullptr;
        PortKind m_Kind;
        std::string m_Name;
        PortDirection m_Direction;
    };
}