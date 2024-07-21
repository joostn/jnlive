#include "utils.h"

namespace {
    constexpr auto invalidmarker = (char32_t)0xfffd;

    void u32u8single(std::string &utf8_str, char32_t c)
    {
        // 0xD800–0xDFFF are invalid in UTF-32:
        if( ((c >= 0xd800) && (c <= 0xdfff)) || (c >= 0x110000) || (c < 0) )
        {
            c = invalidmarker;
        }
        if (c <= 0x7f)
        {
            utf8_str += (char)c;
        }
        else if (c <= 0x7ff)
        {
            utf8_str += (char)(0xc0 | (c >> 6));
            utf8_str += (char)(0x80 | (c & 0x3f));
        }
        else if (c <= 0xffff)
        {
            utf8_str += (char)(0xe0 | (c >> 12));
            utf8_str += (char)(0x80 | ((c >> 6) & 0x3f));
            utf8_str += (char)(0x80 | (c & 0x3f));
        }
        else if(c <= 0x10ffff)
        {
            utf8_str += (char)(0xf0 | (c >> 18));
            utf8_str += (char)(0x80 | ((c >> 12) & 0x3f));
            utf8_str += (char)(0x80 | ((c >> 6) & 0x3f));
            utf8_str += (char)(0x80 | (c & 0x3f));
        }
    }
        
    void u32wsingle(std::wstring &utf16_str, char32_t c)
    {
        // 0xD800–0xDFFF are invalid in UTF-32:
        if( ((c >= 0xd800) && (c <= 0xdfff)) || (c >= 0x110000) || (c < 0) )
        {
            c = invalidmarker;
        }
        if(c <= 0xffff)
        {
            utf16_str += (wchar_t)c;
        }
        else
        {
            c -= 0x10000;
            utf16_str += (wchar_t)(0xd800 | (c >> 10));
            utf16_str += (wchar_t)(0xdc00 | (c & 0x3ff));
        }
    }

    char32_t wu32single(std::wstring_view &utf16_str)
    {
        if(utf16_str.empty()) return 0;
        auto c1 = static_cast<char32_t>(utf16_str.front());
        utf16_str.remove_prefix(1);
        if((c1 & 0xfc00) == 0xd800)
        {
            // this is a lead surrogate:
            if(utf16_str.empty()) return invalidmarker; // unexpected end of string
            auto c2 = static_cast<char32_t>(utf16_str.front());
            utf16_str.remove_prefix(1);
            if(c2 & 0xffff0000)
            {
                return invalidmarker; // can happen if wchar_t is 32 bits
            }
            else if((c2 & 0xfc00) != 0xdc00)
            {
                // this is an error, a lead surrogate without a trail surrogate
                return invalidmarker;
            }
            else
            {
                // this is a valid surrogate pair:
                return (((c1 & 0x3ff) << 10) | (c2 & 0x3ff)) + 0x10000;
            }
        }
        else if((c1 & 0xfc00) == 0xdc00)
        {
            // this is an error, a trail surrogate without a lead surrogate
            return invalidmarker;
        }
        else
        {
            // this is a valid single character:
            return c1;
        }
    }

    char32_t u8u32single(std::string_view &utf8_str)
    {
        if(utf8_str.empty()) return 0;
        auto c1 = static_cast<char32_t>(utf8_str.front());
        utf8_str.remove_prefix(1);
        if((c1 & 0xc0) == 0x80)
        {
            // this is an error, a continuation byte without a lead byte
            return invalidmarker;
        }
        else
        {
            size_t numcontinuations = 0;
            char32_t thischar = 0;
            if((c1 & 0x80) == 0)
            {
                // 1-byte sequence:
                // numcontinuations = 0;
                thischar = c1;
            }
            else if((c1 & 0xe0) == 0xc0)
            {
                // 2-byte sequence:
                numcontinuations = 1;
                thischar = c1 & 0x1f;
            }
            else if((c1 & 0xf0) == 0xe0)
            {
                // 3-byte sequence:
                numcontinuations = 2;
                thischar = c1 & 0x0f;
            }
            else if((c1 & 0xf8) == 0xf0)
            {
                // 4-byte sequence:
                numcontinuations = 3;
                thischar = c1 & 0x07;
            }
            else
            {
                // invalid lead byte:
                return invalidmarker;
            }
            while(numcontinuations > 0)
            {
                if(utf8_str.empty()) return invalidmarker; // unexpected end of string
                auto c = static_cast<char32_t>(utf8_str.front());
                utf8_str.remove_prefix(1);
                if((c & 0xc0) != 0x80)
                {
                // invalid continuation byte:
                return invalidmarker;
                }
                thischar <<= 6;
                thischar |= (c & 0x3f);
                numcontinuations--;
            }
            // 0xD800–0xDFFF are invalid in UTF-32:
            if((thischar >= 0xd800) && (thischar <= 0xdfff))
            {
                return invalidmarker;
            }
            return thischar;
        }
    }
}

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

    TEventLoop::~TEventLoop()  noexcept(false)
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
            auto func = [this](){
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
            };
            m_Thread = std::thread(func);
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
        //return std::regex();
    }

    std::string wu8(std::wstring_view utf16_str)
    {
        std::string result;
        result.reserve(utf16_str.size());
        while(!utf16_str.empty())
        {
            auto c32 = wu32single(utf16_str);
            u32u8single(result, c32);
        }
        return result;
    }

    std::wstring u8w(std::string_view utf8_str)
    {
        std::wstring result;
        result.reserve(utf8_str.size());
        while(!utf8_str.empty())
        {
            auto c32 = u8u32single(utf8_str);
            u32wsingle(result, c32);
        }
        return result;
    }


}