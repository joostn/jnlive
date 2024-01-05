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

}
