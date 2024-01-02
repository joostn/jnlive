#include "log.h"
#include "lilvutils.h"

namespace logger
{
        Logger::Logger()
        {
            m_Urid_Log_Error = lilvutils::World::Static().UriMapLookup(LV2_LOG__Error);
            m_Urid_Log_Trace = lilvutils::World::Static().UriMapLookup(LV2_LOG__Trace);
            m_Urid_Log_Warning = lilvutils::World::Static().UriMapLookup(LV2_LOG__Warning);
        }

}
