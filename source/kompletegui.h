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
        static constexpr auto sDiscardSelectedPresetIndexAfter = std::chrono::seconds(10);
        enum class TMode {Performance, Midi, Controller};

        std::optional<size_t> m_FocusedPart;
        bool m_Shift = false;
        std::array<bool, 9> m_TouchingRotary {false, false, false, false, false, false, false, false, false};

        // Performance mode:
        TMode m_Mode = TMode::Performance;
        std::optional<size_t> GetSelectedPresetIndex() const;
        std::optional<size_t> ActivePresetIndexForFocusedPart() const;
        void SetSelectedPresetIndex(std::optional<size_t> index);

        bool m_ShowPresetList = false;
        //size_t m_QuickPresetPage = 0;
        std::vector<size_t> m_Part2QuickPresetPage;
        float m_OutputLevel = - std::numeric_limits<float>::infinity();
        float m_OutputPeakLevel = - std::numeric_limits<float>::infinity();

        std::pair<int, int> VisiblePartVolumeSliders() const
        {
            return GetPartRangeAroundFocusedPart(7);
        }

        std::pair<int, int> VisiblePartPresetNames() const
        {
            return GetPartRangeAroundFocusedPart(4);
        }

        // Midi mode:
        int m_ProgramChange = 0;

        const engine::Engine::TData &EngineData() const
        {
            return m_EngineData;
        }
        void SetEngineData(const engine::Engine::TData &engineData);
        std::optional<size_t> ActivePartIsHammond() const;
        bool operator==(const TGuiState &other) const = default;
        void SetFocusedPart(std::optional<size_t> focusedPart);
        std::optional<std::chrono::steady_clock::time_point> NextScreenUpdateNeeded() const;

    private:
        engine::Engine::TData m_EngineData;
        std::optional<size_t> m_SelectedPresetIndex;
        std::chrono::steady_clock::time_point m_LastSelectedPresetChange;
        std::pair<int, int> GetPartRangeAroundFocusedPart(int num) const
        {
            int numparts = (int)EngineData().Project().Parts().size();
            int focusedpart = m_FocusedPart.value_or(0);
            int first = std::max(0, focusedpart - num / 2);
            int last = std::min(numparts, first + num);
            return {first, last};
        }

    };

    class TDeviceParams
    {
    public:
        TDeviceParams(std::pair<int, int> &&vidPid, std::string &&serial) : m_Serial(std::move(serial)), m_VidPid(std::move(vidPid)) {}
        const std::pair<int, int>& VidPid() const { return m_VidPid; }
        const std::string& Serial() const { return m_Serial; }
        auto operator<=>(const TDeviceParams &) const = default;
        
    private:
        std::pair<int, int> m_VidPid;
        std::string m_Serial;
    };

    class Gui
    {
    public:
        static constexpr int sFontSize = 15;
        static constexpr int sLineSpacing = 2;
        Gui(const TDeviceParams &deviceParams, engine::Engine &engine);
        ~Gui();
        void Run(); // may cause Connected() to switch to false
        const TGuiState& GuiState() const {return m_GuiState1;}
        void SetGuiState(TGuiState &&state);
        bool Connected() const;
        const TDeviceParams DeviceParams() const {return m_DeviceParams;}

    private:
        void SetWindow(std::unique_ptr<simplegui::Window> window);
        void PaintWindow(Display &display, const simplegui::Window &window, const utils::TIntRegion &dirtyregion);
        void RunGuiThread(std::pair<int, int> vidPid, std::string_view serial);
        void OnButton(Hid::TButtonIndex button, int delta);
        void OnDataChanged();
        void RefreshLcd();
        void RefreshLeds();
        void PaintPerformanceWindow(simplegui::Window &window) const;
        void PaintMidiWindow(simplegui::Window &window) const;
        void PaintControllerWindow(simplegui::Window &window) const;
        void PaintHammondControllerWindow(simplegui::Window &window, size_t part) const;
        void OnOutputLevelChanged();

    private:
        TDeviceParams m_DeviceParams;
        Hid m_Hid;
        std::thread m_GuiThread;
        mutable std::mutex m_Mutex;

        // protected by mutex:
        bool m_DisplayConnected = false;
        std::condition_variable m_WakeGuiThreadCondition;
        std::unique_ptr<simplegui::Window> m_NewWindow;
        std::unique_ptr<simplegui::Window> m_PrevWindow;
        bool m_AbortRequested = false;

        engine::Engine &m_Engine;
        utils::NotifySink m_OnProjectChanged;
        TGuiState m_GuiState1;
        bool m_GuiStateNoRecurse = false;
        utils::NotifySink m_OnOutputLevelUpdate;

        utils::THysteresis m_SelectedPresetHysteresis {10, 20};
        utils::THysteresis m_ProgramChangeHysteresis {10, 20};
        std::array<utils::THysteresis, 8> m_VolumeHysteresis {
            utils::THysteresis(10, 20),
            utils::THysteresis(10, 20),
            utils::THysteresis(10, 20),
            utils::THysteresis(10, 20),
            utils::THysteresis(10, 20),
            utils::THysteresis(10, 20),
            utils::THysteresis(10, 20),
            utils::THysteresis(10, 20)
        };
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
        std::array<utils::THysteresis, 8> m_ControllerHysteresis {
            utils::THysteresis(3, 8),
            utils::THysteresis(3, 8),
            utils::THysteresis(3, 8),
            utils::THysteresis(3, 8),
            utils::THysteresis(3, 8),
            utils::THysteresis(3, 8),
            utils::THysteresis(3, 8),
            utils::THysteresis(3, 8)
        };
        std::optional<std::chrono::steady_clock::time_point> m_NextScheduledLcdRefresh;
        std::chrono::steady_clock::time_point m_LastPeakLevelUpdate;

    };

    class TGuiPool
    {
    public:
        TGuiPool(engine::Engine &engine);
        void Run();

    private:
        void DiscoverDevices();

    private:
        std::map<TDeviceParams, std::unique_ptr<Gui>> m_Device2Gui;
        std::chrono::steady_clock::time_point m_LastDeviceDiscovery;
        engine::Engine &m_Engine;
    };
}