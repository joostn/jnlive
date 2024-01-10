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
}