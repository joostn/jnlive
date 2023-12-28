#include "jackutils.h"
#include "lilvjacklink.h"
#include <thread>

using namespace std::chrono_literals;

namespace jackutils
{
    void Client::BlockProcess()
    {
        m_ProcessBlockCount++;
        if(m_ProcessBlockCount == 1)
        {
            jack_set_process_callback(m_Client, &Client::processStaticDummy, this);
            while(m_Processing)
            {
                std::this_thread::sleep_for(1ms);
            }
        }
    }
    void Client::UnblockProcess()
    {
        m_ProcessBlockCount--;
        if(m_ProcessBlockCount == 0)
        {
            jack_set_process_callback(m_Client, &Client::processStatic, this);
        }
    }
    int Client::processStaticDummy(jack_nframes_t nframes, void* arg)
    {
        // do nothing
        return 0;
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
        m_Processing = true;
        utils::finally fin1([&](){
            m_Processing = false;
        });

        auto jacklinks = lilvjacklink::Global::StaticPtr();
        if(jacklinks)
        {
            jacklinks->process(nframes);
        }
        return 0;
    }
}
