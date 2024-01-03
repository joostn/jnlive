#pragma once

#include <stdarg.h>
#include <stdio.h>
#include "lv2.h"
#include "lv2/log/log.h"

namespace lilvutils
{
    class Instance;
}

namespace logger
{
    class Logger
    {
    public:
        Logger(lilvutils::Instance &instance);
        const LV2_Feature* Feature() const { return &m_LogFeature; }
        static int vprintf_static(LV2_Log_Handle handle, LV2_URID type, const char* fmt, va_list ap)
        {
            auto logger = (Logger*)handle;
            return logger->vprintf(type, fmt, ap);
        }
        static int printf_static(LV2_Log_Handle handle, LV2_URID type, const char* fmt, ...)
        {
            va_list args;
            va_start(args, fmt);
            const int ret = vprintf_static(handle, type, fmt, args);
            va_end(args);
            return ret;
        }

    private:
        int vprintf(LV2_URID type, const char* fmt, va_list ap)
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
    private:
        lilvutils::Instance &m_Instance;
        bool m_EnableTracing = true;
        LV2_URID m_Urid_Log_Error = 0;
        LV2_URID m_Urid_Log_Warning = 0;
        LV2_URID m_Urid_Log_Trace = 0;
        LV2_Log_Log m_Log;
        LV2_Feature m_LogFeature;

    };
}