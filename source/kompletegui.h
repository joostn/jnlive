#pragma once

#include "komplete.h"
#include <thread>
#include <mutex>

namespace komplete
{
    class Gui
    {
    public:
        Gui(std::pair<int, int> vidPid);
        ~Gui();
        void Run();
        void RunGuiThread();

    private:
        Display m_Display;
        Hid m_Hid;
        std::thread m_GuiThread;
        bool m_AbortRequested = false;
        std::mutex m_Mutex;
    };
}