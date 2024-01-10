#include "log.h"
#include "lilvutils.h"

namespace logger
{
        Logger::Logger() 
        {
            m_Urid_Log_Error = lilvutils::World::Static().UriMapLookup(LV2_LOG__Error);
            m_Urid_Log_Trace = lilvutils::World::Static().UriMapLookup(LV2_LOG__Trace);
            m_Urid_Log_Warning = lilvutils::World::Static().UriMapLookup(LV2_LOG__Warning);
            m_Log.handle  = this;
            m_Log.printf  = &logger::Logger::printf_static;
            m_Log.vprintf = &logger::Logger::vprintf_static;
            m_LogFeature.data = &m_Log;
            m_LogFeature.URI = LV2_LOG__log;
        }

        int Logger::vprintf_static(LV2_Log_Handle handle, LV2_URID type, const char* fmt, va_list ap)
        {
            auto logger = (Logger*)handle;
            return logger->vprintf(type, fmt, ap);
        }
        
        int Logger::printf_static(LV2_Log_Handle handle, LV2_URID type, const char* fmt, ...)
        {
            va_list args;
            va_start(args, fmt);
            const int ret = vprintf_static(handle, type, fmt, args);
            va_end(args);
            return ret;
        }

        int Logger::vprintf(LV2_URID type, const char* fmt, va_list ap)
        {
            if ( (type == m_Urid_Log_Trace) && m_EnableTracing)            
            {
                fprintf(stderr, "trace: ");
                vfprintf(stderr, fmt, ap);
                fprintf(stderr, "\n");
            }
            else if (type == m_Urid_Log_Error) 
            {
                fprintf(stderr, "error: ");
                vfprintf(stderr, fmt, ap);
                fprintf(stderr, "\n");
            }
            else if (type == m_Urid_Log_Warning)
            {
                fprintf(stderr, "error: ");
                vfprintf(stderr, fmt, ap);
                fprintf(stderr, "\n");
            }
            else // note:
            {
                fprintf(stderr, "note: ");
                vfprintf(stderr, fmt, ap);
                fprintf(stderr, "\n");
            }
            return 0;
        }

}
