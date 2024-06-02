#pragma once

#include "komplete.h"
#include <thread>
#include <mutex>

namespace simplegui
{
    class Window;
}

namespace komplete
{
    class Gui
    {
    public:
        Gui(std::pair<int, int> vidPid);
        ~Gui();
        void Run();
        void SetWindow(std::unique_ptr<simplegui::Window> window);

    private:
        void PaintWindow(Display &display, const simplegui::Window &window);
        void RunGuiThread(std::pair<int, int> vidPid);
        void OnButton(Hid::TButtonIndex button, int delta);

    private:
        Hid m_Hid;
        std::thread m_GuiThread;
        std::mutex m_Mutex;
        // protected by mutex:
        std::unique_ptr<simplegui::Window> m_NewWindow;
        std::unique_ptr<simplegui::Window> m_PrevWindow;
        bool m_AbortRequested = false;

        int m_LedIndex = 0;
        int m_LedValue = 0;
    };
}