#include "komplete.h"
#include <thread>
#include "kompletegui.h"

using std::chrono_literals::operator""ms;

namespace komplete
{
    Gui::Gui(std::pair<int, int> vidPid) : m_Display(vidPid), m_Hid(vidPid)
    {
        m_GuiThread = std::thread([this]() {
            RunGuiThread();
        });
    }

    Gui::~Gui()
    {
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_AbortRequested = true;
        }
        m_GuiThread.join();
    }

    void Gui::Run()
    {
        m_Hid.Run();
    }

    void Gui::RunGuiThread()
    {
        while(true)
        {
            {
                std::unique_lock<std::mutex> lock(m_Mutex);
                if(m_AbortRequested)
                {
                    return;
                }
            }
            m_Display.Run();
            std::this_thread::sleep_for(100ms);
        }
    }
}
