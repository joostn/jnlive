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
        const void* Data() const { return m_Data.data(); }
        uint32_t DataSize() const { return m_DataSize; }
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
    Worker::~Worker()
    {
        Stop();
    }
    LV2_Worker_Status Worker::schedule_work(uint32_t size, const void* data)
    {
        // called in audio thread:
        if(ScheduleMessage::cMaxDataSize >= size)
        {
            bool success = m_RingBufFromRealtimeThread.Write(ScheduleMessage(size, data), false);
            if(success)
            {
                return LV2_WORKER_SUCCESS;
            }
        }
        return LV2_WORKER_ERR_NO_SPACE;
    }
    void Worker::RunInRealtimeThread()
    {
        // called in audio thread:
        while(true)
        {
            auto message = m_RingBufFromRealtimeThread.Read();
            if(!message) break;
            if(auto schedulemessage = dynamic_cast<const ScheduleMessage*>(message))
            {
                if(m_WorkerInterface && m_WorkerInterface->work_response)
                {
                    m_WorkerInterface->work_response(m_Instance.get(), schedulemessage->DataSize(), schedulemessage->Data());
                }
            }
        }
        if(m_WorkerInterface->end_run)
        {
            m_WorkerInterface->end_run(m_Instance.get());
        }
    }
    void Worker::WorkerThreadFunc()
    {
        while(true)
        {
            {
                std::unique_lock lk(m_Mutex);
                m_Cv.wait_for(lk, std::chrono::milliseconds(1), [this]{ return m_AbortRequested; });
                if(m_AbortRequested)
                {
                    break;
                }
            }
            while(true)
            {
                auto message = m_RingBufFromRealtimeThread.Read();
                if(!message) break;
                if(auto schedulemessage = dynamic_cast<const ScheduleMessage*>(message))
                {
                    if(m_WorkerInterface && m_WorkerInterface->work)
                    {
                        m_WorkerInterface->work(m_Instance.get(), &Worker::RespondStatic, this, schedulemessage->DataSize(), schedulemessage->Data());
                    }
                }
            }

        }
    }

    void Worker::Start(const LV2_Worker_Interface* iface)
    {
        m_WorkerInterface = iface;
        if(m_WorkerInterface)
        {
            m_WorkerThread = std::thread([this](){
                WorkerThreadFunc();
            });
        }
    }

    void Worker::Stop()
    {
        if(m_WorkerInterface)
        {
            {
                std::unique_lock lk(m_Mutex);
                m_AbortRequested = true;
            }
            m_Cv.notify_all();
            m_WorkerThread.join();
            m_WorkerInterface = nullptr;
        }
    }

    /* static */ LV2_Worker_Status Worker::RespondStatic(LV2_Worker_Respond_Handle handle, uint32_t size, const void *data)
    {
        return static_cast<Worker*>(handle)->Respond(size, data);
    }

    LV2_Worker_Status Worker::Respond(uint32_t size, const void *data)
    {
        // called in worker thread:
        if(ScheduleMessage::cMaxDataSize >= size)
        {
            bool success = m_RingBufToRealtimeThread.Write(ScheduleMessage(size, data), false);
            if(success)
            {
                return LV2_WORKER_SUCCESS;
            }
        }
        return LV2_WORKER_ERR_NO_SPACE;
    }

}
