#pragma once
#include "lv2/state/state.h"
#include "lv2/worker/worker.h"
//#include "ringbuf.h"
#include <thread>
#include <condition_variable>
#include <mutex>

import ringbuf;

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
        ~Worker();
        LV2_Worker_Status schedule_work(uint32_t size, const void* data);
        void Start(const LV2_Worker_Interface* iface);
        void Stop();
        void RunInRealtimeThread();
        const LV2_Feature& ScheduleFeature() const { return m_ScheduleFeature; }
        void WorkerThreadFunc();
        static LV2_Worker_Status RespondStatic(LV2_Worker_Respond_Handle handle, uint32_t size, const void *data);

    private:
        LV2_Worker_Status Respond(uint32_t size, const void *data);
        
    private:
        std::thread m_WorkerThread;
        std::mutex m_Mutex;
        std::condition_variable m_Cv;
        bool m_AbortRequested = false;
        const LV2_Worker_Interface* m_WorkerInterface = nullptr;
        LV2_Worker_Schedule m_Schedule;
        LV2_Feature m_ScheduleFeature;
        lilvutils::Instance& m_Instance;
        ringbuf::RingBuf m_RingBufFromRealtimeThread;
        ringbuf::RingBuf m_RingBufToRealtimeThread;
    };
}
