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
        enum class TMode {Performance, Midi, Controller};

        std::optional<size_t> m_FocusedPart;
        bool m_Shift = false;

        // Performance mode:
        TMode m_Mode = TMode::Performance;
        size_t m_SelectedPreset = 0;
        bool m_ShowPresetList = false;
        size_t m_QuickPresetPage = 0;

        // Midi mode:
        int m_ProgramChange = 0;

        // Controller mode:
        const engine::Engine::TData &EngineData() const
        {
            return m_EngineData;
        }
        void SetEngineData(const engine::Engine::TData &engineData);
        std::optional<size_t> ActivePartIsHammond() const;
    private:
        engine::Engine::TData m_EngineData;

    };
    class Gui
    {
    public:
        static constexpr int sFontSize = 15;
        static constexpr int sLineSpacing = 2;
        Gui(std::pair<int, int> vidPid, engine::Engine &engine);
        ~Gui();
        void Run();
        const TGuiState& GuiState() const {return m_GuiState1;}
        void SetGuiState(TGuiState &&state);

    private:
        void SetWindow(std::unique_ptr<simplegui::Window> window);
        void PaintWindow(Display &display, const simplegui::Window &window);
        void RunGuiThread(std::pair<int, int> vidPid);
        void OnButton(Hid::TButtonIndex button, int delta);
        void OnDataChanged();
        void RefreshLcd();
        void RefreshLeds();
        void PaintPerformanceWindow(simplegui::Window &window) const;
        void PaintMidiWindow(simplegui::Window &window) const;
        void PaintControllerWindow(simplegui::Window &window) const;
        void PaintHammondControllerWindow(simplegui::Window &window, size_t part) const;

    private:
        Hid m_Hid;
        std::thread m_GuiThread;
        std::mutex m_Mutex;
        // protected by mutex:
        std::unique_ptr<simplegui::Window> m_NewWindow;
        std::unique_ptr<simplegui::Window> m_PrevWindow;
        bool m_AbortRequested = false;
        engine::Engine &m_Engine;
        utils::NotifySink m_OnProjectChanged {m_Engine.OnDataChanged(), [this](){OnDataChanged();}};
        TGuiState m_GuiState1;

        utils::THysteresis m_SelectedPresetHysteresis {10, 20};
        utils::THysteresis m_ReverbLevelHysteresis {10, 20};
        utils::THysteresis m_Part1LevelHysteresis {10, 20};
        utils::THysteresis m_Part2LevelHysteresis {10, 20};
        utils::THysteresis m_ProgramChangeHysteresis {10, 20};
        std::array<utils::THysteresis, 9> m_DrawbarHysteresis {
            utils::THysteresis(20, 40),
            utils::THysteresis(20, 40),
            utils::THysteresis(20, 40),
            utils::THysteresis(20, 40),
            utils::THysteresis(20, 40),
            utils::THysteresis(20, 40),
            utils::THysteresis(20, 40),
            utils::THysteresis(20, 40),
            utils::THysteresis(0,1)
        };


    };
}