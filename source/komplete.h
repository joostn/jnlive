#pragma once

#include <vector>
#include <iostream>
#include <cstring>
#include <chrono>

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
        Hid(const Hid&) = delete;
        Hid& operator=(const Hid&) = delete;
        Hid(Hid&&) = delete;
        Hid& operator=(Hid&&) = delete;
        Hid(std::pair<int, int> vidPid);
        void Connect();
        void TryConnectSometimes();
        void Disconnect();
        void Run();
        ~Hid();
        bool Connected() const
        {
            return m_Device != nullptr;
        }
    private:
        hid_device* m_Device = nullptr;
        std::chrono::time_point<std::chrono::system_clock> m_LastConnectTime = std::chrono::system_clock::now();
        std::pair<int, int> m_VidPid;
    };

    class Display
    {
    public:
        static constexpr int sWidth = 480 * 2;
        static constexpr int sHeight = 272;
        static constexpr int sBytesPerPixel = 2;
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
        static constexpr int sDisplayBufferStride = sBytesPerPixel * sWidth;
        std::vector<unsigned char> m_DisplayBuffer = std::vector<unsigned char>((size_t)sHeight*sDisplayBufferStride);
        static constexpr int sInterfaceNumber = 3;
     	libusb_context *m_Context = nullptr;
        libusb_device_handle* m_Device = nullptr;
        std::chrono::time_point<std::chrono::system_clock> m_LastConnectTime = std::chrono::system_clock::now();
        std::pair<int, int> m_VidPid;
    };
}