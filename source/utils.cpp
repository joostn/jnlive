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
}