#pragma once
#include <functional>

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

}