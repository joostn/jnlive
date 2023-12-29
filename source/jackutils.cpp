#include "jackutils.h"
#include "lilvjacklink.h"
#include <thread>

using namespace std::chrono_literals;

namespace jackutils
{
    void Client::BlockProcess()
    {
        bool expected = false;
        while(!m_RealTimeLock.compare_exchange_strong(expected, true))
        {
            std::this_thread::sleep_for(1ms);
        }
    }
    void Client::UnblockProcess()
    {
        m_RealTimeLock = false;
    }
    int Client::processStatic(jack_nframes_t nframes, void* arg)
    {
        auto theclient = (Client*)arg;
        if (theclient)
        {
            return theclient->process(nframes);
        }
        return 0;
    }
    int Client::process(jack_nframes_t nframes)
    {
        bool expected = false;
        if(!m_RealTimeLock.compare_exchange_strong(expected, true))
        {
            // just skip this cycle
            return 0;
        }
        utils::finally fin1([&](){
            m_RealTimeLock = false;
        });

        auto jacklinks = lilvjacklink::Global::StaticPtr();
        if(jacklinks)
        {
            jacklinks->process(nframes);
        }
        return 0;
    }
}
