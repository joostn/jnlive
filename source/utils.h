#pragma once
#include <functional>
#include <set>
#include <string>

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
        void Notify() const
        {
            m_Func();
        }
    private:
        std::function<void(void)> m_Func;
        NotifySource *m_Source = nullptr;
    };

    std::string generate_random_tempdir();

}