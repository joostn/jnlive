#pragma once
#include <functional>
#include <set>
#include <string>
#include <thread>
#include <mutex>
#include <gtkmm.h>
#include <condition_variable>
#include <regex>

namespace utils
{
    class finally
    {
    public:
        typedef std::function<void(void)> TFunc;
        inline finally(TFunc &&func)
            : m_Func(std::move(func)) {}
        inline ~finally() noexcept(false)
        {
            if(!m_Dismissed)
            {
                m_Func();
            }
        }
        void Dismiss() noexcept(true)
        {
            m_Dismissed = true;
        }

        private:
        TFunc m_Func;
        bool m_Dismissed = false;
    };

    class NotifySink;
    class NotifySource
    {
        friend NotifySink;
    public:
        NotifySource(const NotifySource&) = delete;
        NotifySource& operator=(const NotifySource&) = delete;
        NotifySource(NotifySource&&) = delete;
        NotifySource& operator=(NotifySource&&) = delete;
        NotifySource() {}
        ~NotifySource();
        void Notify() const;
    private:
        void AddSink(NotifySink *sink)
        {
            m_Sinks.insert(sink);
        }
        void RemoveSink(NotifySink *sink)
        {
            m_Sinks.erase(sink);
        }    
        std::set<NotifySink*> m_Sinks;
    };

    class NotifySink
    {
        friend NotifySource;
    public:
        NotifySink(const NotifySink&) = delete;
        NotifySink& operator=(const NotifySink&) = delete;
        NotifySink(NotifySource &source, std::function<void(void)> &&func) : m_Func(std::move(func)), m_Source(&source)
        {
            m_Source->AddSink(this);
        }
        NotifySink(NotifySink &&src) : m_Func(std::move(src.m_Func)), m_Source(src.m_Source)
        {
            src.m_Source = nullptr;
        }
        NotifySink& operator=(NotifySink&&) = delete;
        ~NotifySink()
        {
            if(m_Source)
            {
                m_Source->RemoveSink(this);
            }
        }

    private:
        void Notify() const
        {
            m_Func();
        }

    private:
        std::function<void(void)> m_Func;
        NotifySource *m_Source = nullptr;
    };

    class THysteresis
    {
    public:
        THysteresis(int hyst, int reduction) : m_Hyst(hyst), m_Reduction(reduction) {}
        int Update(int delta)
        {
            int reduced = 0;
            m_Value += delta;
            if(m_Value > m_Hyst)
            {
                auto v = m_Value - m_Hyst;
                reduced = v / m_Reduction;
                m_Value -= reduced * m_Reduction;
            }
            else if(m_Value < -m_Hyst)
            {
                auto v = m_Value + m_Hyst;
                reduced = v / m_Reduction;
                m_Value -= reduced * m_Reduction;
            }
            return reduced;            
        }
        
    private:
        int m_Hyst;
        int m_Reduction;
        int m_Value = 0;
    };

    std::string generate_random_tempdir();
    // make a regular expression from a string
    // '*' matches any substring
    // '?' matches any character
    std::regex makeSimpleRegex(std::string_view s);

    class TEventLoop;

    class TEventLoopAction
    {
        friend TEventLoop;
    public:
        TEventLoopAction(const TEventLoopAction&) = delete;
        TEventLoopAction& operator=(const TEventLoopAction&) = delete;
        TEventLoopAction(TEventLoopAction&&) = delete;
        TEventLoopAction& operator=(TEventLoopAction&&) = delete;
        ~TEventLoopAction();
        TEventLoopAction(TEventLoop &eventloop, std::function<void(void)> &&func);
        void Signal(); // can be called from other thread
    private:
        std::function<void(void)> m_Func;
        TEventLoop &m_EventLoop;
    };

    // Abstract base class. Use TGtkAppEventLoop or TThreadWithEventLoop
    class TEventLoop
    {
        friend TEventLoopAction;
    public:
        TEventLoop(const TEventLoop&) = delete;
        TEventLoop& operator=(const TEventLoop&) = delete;
        TEventLoop(TEventLoop&&) = delete;
        TEventLoop& operator=(TEventLoop&&) = delete;
        TEventLoop();
        virtual ~TEventLoop() noexcept(false);
        void CheckThreadId() const;
        void ProcessPendingMessages();

    protected:
        void ActionAdded(TEventLoopAction *action);
        void ActionRemoved(TEventLoopAction *action);
        virtual void ActionSignaled(TEventLoopAction *action) = 0;
        void Call(TEventLoopAction *action)
        {
            action->m_Func();
        }

    protected:
        std::thread::id m_OwningThreadId;
        size_t m_NumActions = 0;
        std::mutex m_Mutex;
        std::set<TEventLoopAction*> m_SignaledActions;
    };

    // Event loops which uses the GTK eventloop
    class TGtkAppEventLoop : public TEventLoop
    {
    public:
        TGtkAppEventLoop();
        virtual ~TGtkAppEventLoop();

    protected:
        virtual void ActionSignaled(TEventLoopAction *action) override;

    private:
        Glib::Dispatcher m_Dispatcher;
    };

    // Event loop which uses a separate thread
    class TThreadWithEventLoop : public TEventLoop
    {
    public:
        TThreadWithEventLoop();
        virtual ~TThreadWithEventLoop();
        void Run();
        void Stop();

    protected:
        virtual void ActionSignaled(TEventLoopAction *action) override;

    private:
        std::thread m_Thread;
        std::condition_variable m_QueueCond;
        bool m_RequestTerminate = false;
    };
}