#pragma once
#include "lv2/state/state.h"
#include "lv2/worker/worker.h"
#include "ringbuf.h"
#include <thread>

namespace lilvutils
{
    class Instance;
}

namespace schedule
{
    class Worker
    {
    public:
        Worker(lilvutils::Instance &instance);
        LV2_Worker_Status schedule_work(uint32_t size, const void* data);
        void Start(const LV2_Worker_Interface* iface);
        void RunInRealtimeThread();
        const LV2_Feature& ScheduleFeature() const { return m_ScheduleFeature; }
        void WorkerThreadFunc();

    private:
        std::thread m_WorkerThread;
        const LV2_Worker_Interface* m_WorkerInterface = nullptr;
        LV2_Worker_Schedule m_Schedule;
        LV2_Feature m_ScheduleFeature;
        lilvutils::Instance& m_Instance;
        ringbuf::RingBuf m_RingBufFromRealtimeThread;
        ringbuf::RingBuf m_RingBufToRealtimeThread;
    };
}
