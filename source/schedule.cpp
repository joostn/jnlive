#include "schedule.h"
#include "lilvutils.h"
#include <string.h>

// https://lv2plug.in/c/html/group__worker.html#structLV2__Worker__Schedule

namespace
{
    class ScheduleMessage : public ringbuf::PacketBase
    {
    public:
        static constexpr uint32_t cMaxDataSize = 4096;
        ScheduleMessage(uint32_t size, const void* data) : m_DataSize(size)
        {
            memcpy(m_Data.data(), data, size);
        }
    private:
        std::array<char, cMaxDataSize> m_Data;
        uint32_t m_DataSize;
    };

}

namespace schedule
{       
    Worker::Worker(lilvutils::Instance &instance) : m_Instance(instance), m_RingBufFromRealtimeThread(130000, sizeof(ScheduleMessage)), m_RingBufToRealtimeThread(130000, sizeof(ScheduleMessage))
    {
        m_Schedule.handle = this;
        m_Schedule.schedule_work = [](LV2_Worker_Schedule_Handle handle, uint32_t size, const void* data) -> LV2_Worker_Status
        {
            return static_cast<Worker*>(handle)->schedule_work(size, data);
        };
        m_ScheduleFeature.data = &m_Schedule;
        m_ScheduleFeature.URI = LV2_WORKER__schedule;
    }
    LV2_Worker_Status Worker::schedule_work(uint32_t size, const void* data)
    {
        // called in audio thread:
        if(ScheduleMessage::cMaxDataSize >= size)
        {
            bool success = m_RingBufFromRealtimeThread.Write(ScheduleMessage(size, data));
            return success? LV2_WORKER_SUCCESS : LV2_WORKER_ERR_NO_SPACE;
        }
        else
        {
            return LV2_WORKER_ERR_NO_SPACE;
        }
    }
    void Worker::RunInRealtimeThread()
    {
        // called in audio thread:
    }
    void Worker::WorkerThreadFunc()
    {
        
    }

    void Worker::Start(const LV2_Worker_Interface* iface)
    {
        m_WorkerInterface = iface;
        m_WorkerThread = std::thread([this](){
            WorkerThreadFunc();
        })
    }


}
