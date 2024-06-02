#pragma once

#include <vector>
#include <iostream>
#include <cstring>
#include <chrono>
#include <functional>

#define LED_BRIGHT          0x7e
#define LED_ON              0x7c
#define LED_OFF             0x00

typedef struct hid_device_ hid_device;
struct libusb_context;
struct libusb_device_handle;

namespace komplete
{
    class Hid
    {
    public:
        enum class TLedColor {
            Off = 0,
            Red = 1,
            Brown = 2,
            Orange = 3,
            Amber = 4,
            Yellow = 5,
            Lime = 6,
            Green = 7,
            Mint = 8,
            Cyan = 9,
            Azure = 10,
            Blue = 11,
            Violet = 12,
            Magenta = 13,
            Purple = 14,
            Pink1 = 15,
            Pink2 = 16,
            White = 17
        };
        enum class TButtonIndex {
            TouchRotary = 45,
            RotaryRight = 40,
            RotaryDown = 41,
            RotaryLeft = 43,
            RotaryUp = 42,
            Browser = 37,
            Plugin = 38,
            Mixer = 39,
            Instance = 35,
            Midi = 34,
            Setup = 36,
            TouchLcdRotary0 = 48,
            TouchLcdRotary1 = 49,
            TouchLcdRotary2 = 50,
            TouchLcdRotary3 = 51,
            TouchLcdRotary4 = 52,
            TouchLcdRotary5 = 53,
            TouchLcdRotary6 = 54,
            TouchLcdRotary7 = 55,
            LcdRotary0 = 72,
            LcdRotary1 = 73,
            LcdRotary2 = 74,
            LcdRotary3 = 75,
            LcdRotary4 = 76,
            LcdRotary5 = 77,
            LcdRotary6 = 78,
            LcdRotary7 = 79,
            Menu0 = 0,
            Menu1 = 1,
            Menu2 = 2,
            Menu3 = 3,
            Menu4 = 4,
            Menu5 = 5,
            Menu6 = 6,
            Menu7 = 7,
            Scene = 29,
            Pattern = 28,
            Track = 27,
            Clear = 26,
            PresetUp = 19,
            PresetDown = 17,
            Left = 16,
            Right = 18,
            Mute = 31,
            Solo = 30,
            Shift = 8,
            Scale = 12,
            Arp = 13,
            Undo = 9,
            Quantize = 14,
            Auto = 15,
            Loop = 10,
            Metro = 20,
            Tempo = 21,
            Play = 11,
            Rec = 22,
            Stop = 23,
            Fixed = 61,
            KeyMode = 25,
        };
        using TOnButton = std::function<void(TButtonIndex button, int delta)>;
        Hid(const Hid&) = delete;
        Hid& operator=(const Hid&) = delete;
        Hid(Hid&&) = delete;
        Hid& operator=(Hid&&) = delete;
        Hid(std::pair<int, int> vidPid, TOnButton &&onButton);
        void Connect();
        void TryConnectSometimes();
        void Disconnect();
        void Run();
        ~Hid();
        bool Connected() const
        {
            return m_Device != nullptr;
        }
        // color 0: off, 1-4: brightness
        void SetLed(TButtonIndex button, TLedColor color, int brightness = 2);
        void SetLed(TButtonIndex button, int brightness)
        {
            SetLed(button, TLedColor::White, brightness);
        }
        void ClearAllLeds();

    private:
        void UpdateButtonState(std::vector<int> &&buttonstate);
        void SendButtonDelta(TButtonIndex button, int delta);
        void UpdateLedState();

    private:
        hid_device* m_Device = nullptr;
        std::chrono::time_point<std::chrono::system_clock> m_LastConnectTime = std::chrono::system_clock::now();
        std::pair<int, int> m_VidPid;
        std::vector<int> m_PrevButtonState;
        TOnButton m_OnButton;
        std::array<uint8_t, 69> m_LedState;
        bool m_LedStateChanged = false;
        std::vector<char> m_LedMap;
    };

    class Display
    {
    public:
        static constexpr int sWidth = 480 * 2;
        static constexpr int sHeight = 272;
        static constexpr int sBytesPerPixel = 2;
        static constexpr int sDisplayBufferStride = sBytesPerPixel * sWidth;
        Display(std::pair<int, int> vidPid);
        void Connect();
        void Disconnect();
        void SendPixels(int x, int y, int width, int height);
        ~Display();
        void TryConnectSometimes();
        bool Connected() const
        {
            return m_Device != nullptr;
        }
        void Run();
        uint16_t* DisplayBuffer(int x = 0, int y = 0) const
        {
            return (uint16_t*)(m_DisplayBuffer.data() + y * sDisplayBufferStride + x * sBytesPerPixel);
        }

    private:
        std::vector<unsigned char> m_DisplayBuffer = std::vector<unsigned char>((size_t)sHeight*sDisplayBufferStride);
        static constexpr int sInterfaceNumber = 3;
     	libusb_context *m_Context = nullptr;
        libusb_device_handle* m_Device = nullptr;
        std::chrono::time_point<std::chrono::system_clock> m_LastConnectTime = std::chrono::system_clock::now();
        std::pair<int, int> m_VidPid;
    };
}