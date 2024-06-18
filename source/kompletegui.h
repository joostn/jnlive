#pragma once

#include "komplete.h"
#include "engine.h"
#include "utils.h"
#include <thread>
#include <mutex>

namespace simplegui
{
    class Window;
}

namespace komplete
{

    class TGuiState
    {
    public:
        enum class TMode {Performance, Midi};
        // all modes:
        bool m_Shift = false;

        // Performance mode:
        TMode m_Mode = TMode::Performance;
        utils::THysteresis m_SelectedPresetHysteresis {10, 20};
        size_t m_SelectedPreset = 0;
        bool m_ShowPresetList = false;
        size_t m_QuickPresetPage = 0;
        utils::THysteresis m_ReverbLevelHysteresis {1, 1};
        utils::THysteresis m_Part1LevelHysteresis {1, 1};
        utils::THysteresis m_Part2LevelHysteresis {1, 1};

        // Midi mode:
        int m_ProgramChange = 0;
        utils::THysteresis m_ProgramChangeHysteresis {10, 20};
    };
    class Gui
    {
    public:
        Gui(std::pair<int, int> vidPid, engine::Engine &engine);
        ~Gui();
        void Run();
        void SetWindow(std::unique_ptr<simplegui::Window> window);

    private:
        void PaintWindow(Display &display, const simplegui::Window &window);
        void RunGuiThread(std::pair<int, int> vidPid);
        void OnButton(Hid::TButtonIndex button, int delta);
        void OnProjectChanged();
        void Refresh();
        void RefreshLeds();

    private:
        Hid m_Hid;
        std::thread m_GuiThread;
        std::mutex m_Mutex;
        // protected by mutex:
        std::unique_ptr<simplegui::Window> m_NewWindow;
        std::unique_ptr<simplegui::Window> m_PrevWindow;
        bool m_AbortRequested = false;
        engine::Engine &m_Engine;
        utils::NotifySink m_OnProjectChanged {m_Engine.OnProjectChanged(), [this](){OnProjectChanged();}};
        TGuiState m_GuiState;
    };
}