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
        Logger();
        const LV2_Feature* Feature() const { return &m_LogFeature; }
        static int vprintf_static(LV2_Log_Handle handle, LV2_URID type, const char* fmt, va_list ap);
        static int printf_static(LV2_Log_Handle handle, LV2_URID type, const char* fmt, ...);
        int vprintf(LV2_URID type, const char* fmt, va_list ap);

    private:
        bool m_EnableTracing = true;
        LV2_URID m_Urid_Log_Error = 0;
        LV2_URID m_Urid_Log_Warning = 0;
        LV2_URID m_Urid_Log_Trace = 0;
        LV2_Log_Log m_Log;
        LV2_Feature m_LogFeature;

    };
}