#include "utils.h"

namespace utils
{
    void NotifySource::Notify() const
    {
        for(auto sink : m_Sinks)
        {
            sink->Notify();
        }
    }
    NotifySource::~NotifySource()
    {
        for(auto sink : m_Sinks)
        {
            sink->m_Source = nullptr;
        }
    }
    std::string generate_random_tempdir() 
    {
        char temp_template[] = "/tmp/jnliveXXXXXX"; // Template for the temp directory
        char* tempdir = mkdtemp(temp_template);

        if (tempdir == nullptr) {
            // Handle the error, mkdtemp failed
            return "";
        }

        // Convert to std::string
        return std::string(tempdir);
    }

    TEventLoop::TEventLoop() : m_OwningThreadId(std::this_thread::get_id())
    {
    }

    TEventLoop::~TEventLoop()
    {
        if(m_NumActions != 0)
        {
            throw std::runtime_error("Destroy TEventLoopAction first");
        }
    }

    void TEventLoop::ActionAdded(TEventLoopAction *action)
    {
        CheckThreadId();
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_NumActions++;
    }

    void TEventLoop::ActionRemoved(TEventLoopAction *action)
    {
        CheckThreadId();
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_NumActions--;
        m_SignaledActions.erase(action);
    }

    void TEventLoop::CheckThreadId() const
    {
        if(std::this_thread::get_id() != m_OwningThreadId)
        {
            throw std::runtime_error("Called from wrong thread");
        }
    }

    void TEventLoop::ProcessPendingMessages()
    {
        CheckThreadId();
        while(true)
        {
            TEventLoopAction* action = nullptr;
            {
                std::unique_lock<std::mutex> lock(m_Mutex);
                if(m_SignaledActions.empty())
                {
                    break;
                }
                action = *m_SignaledActions.begin();
                m_SignaledActions.erase(m_SignaledActions.begin());
            }
            Call(action);
        }
    }

    TEventLoopAction::TEventLoopAction(TEventLoop &eventloop, std::function<void(void)> &&func) : m_Func(std::move(func)), m_EventLoop(eventloop)
    {
        m_EventLoop.ActionAdded(this);
    }
    
    TEventLoopAction::~TEventLoopAction()
    {
        m_EventLoop.ActionRemoved(this);
    }
    
    void TEventLoopAction::Signal()
    {
        m_EventLoop.ActionSignaled(this);
    }
    
    TGtkAppEventLoop::TGtkAppEventLoop() : TEventLoop()
    {
        m_Dispatcher.connect([this](){
            CheckThreadId();
            while(true)
            {
                TEventLoopAction* action = nullptr;
                {
                    std::unique_lock<std::mutex> lock(m_Mutex);
                    if(m_SignaledActions.empty())
                    {
                        break;
                    }
                    action = *m_SignaledActions.begin();
                    m_SignaledActions.erase(m_SignaledActions.begin());
                }
                Call(action);
            }
        });
    }

    TGtkAppEventLoop::~TGtkAppEventLoop()
    {

    }

    void TGtkAppEventLoop::ActionSignaled(TEventLoopAction *action)
    {
        bool mustwakeup = false;
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            mustwakeup = m_SignaledActions.empty();
            m_SignaledActions.insert(action);
        }
        if(mustwakeup)
        {
            m_Dispatcher.emit();
        }
    }

    TThreadWithEventLoop::TThreadWithEventLoop() : TEventLoop()
    {
    }

    void TThreadWithEventLoop::Run()
    {
        if(!m_Thread.joinable())
        {
            m_RequestTerminate = false;
            m_Thread = std::thread([this](){
                m_OwningThreadId = std::this_thread::get_id();
                while(true)
                {
                    TEventLoopAction *action = nullptr;
                    {
                        std::unique_lock<std::mutex> lock(m_Mutex);
                        //         std::condition_variable m_QueueCond:
                        m_QueueCond.wait(lock, [this](){ 
                            return m_RequestTerminate || !m_SignaledActions.empty(); 
                        });
                        if(!m_SignaledActions.empty())
                        {
                            action = *m_SignaledActions.begin();
                            m_SignaledActions.erase(m_SignaledActions.begin());
                        }
                        else if(m_RequestTerminate)
                        {
                            break;
                        }
                    }
                    if(action)
                    {
                        Call(action);
                    }
                }
            });
        }
    }

    void TThreadWithEventLoop::ActionSignaled(TEventLoopAction *action)
    {
        std::unique_lock<std::mutex> lock(m_Mutex);
        bool mustwakeup = m_SignaledActions.empty();
        m_SignaledActions.insert(action);
        if(mustwakeup)
        {
            m_QueueCond.notify_one();
        }
    }

    void TThreadWithEventLoop::Stop()
    {
        if(m_Thread.joinable())
        {
            {
                std::unique_lock<std::mutex> lock(m_Mutex);
                m_RequestTerminate = true;
                m_QueueCond.notify_one();
            }
            m_Thread.join();
            m_OwningThreadId = std::this_thread::get_id();
        }
    }

    TThreadWithEventLoop::~TThreadWithEventLoop()
    {
        Stop();
    }

    std::regex makeSimpleRegex(std::string_view s)
    {
        std::string regexPattern;
        for (char ch : s) 
        {
            if (ch == '*') 
            {
                regexPattern += ".*";
            } 
            else if (ch == '?') 
            {
                regexPattern += ".";
            }
            else if ( (ch == '.') || (ch == '[') || (ch == '\\') || (ch == '^') || (ch == '$') ) 
            {
                // https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap09.html#tag_09_03_03
                regexPattern += "\\";
                regexPattern += ch;
            } 
            else 
            {
                regexPattern += ch;
            }
        }
        return std::regex(regexPattern, std::regex_constants::basic);
    }
 
}