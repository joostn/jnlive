#pragma once

#include <hidapi.h>
#include <libusb.h>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <cstring>
#include <chrono>

#define LED_BRIGHT          0x7e
#define LED_ON              0x7c
#define LED_OFF             0x00

namespace komplete
{
    class Hid
    {
    public:
        Hid(const Hid&) = delete;
        Hid& operator=(const Hid&) = delete;
        Hid(Hid&&) = delete;
        Hid& operator=(Hid&&) = delete;
        Hid()
        {
            auto err = hid_init();
            if(err)
            {
                throw std::runtime_error("Failed to init hidapi");
            }
        }
        void Connect()
        {
            if(!m_Device)
            {
                m_Device = hid_open(0x17cc, 0x1620, nullptr);
                if(m_Device)
                {
                    hid_set_nonblocking( m_Device, 0 );

                    unsigned char hid_packet_interactive[] = { 0xa0, 0x03, 0x04 };
                    unsigned char hid_packet_midi_mode[] = { 0xa0, 0x07, 0x00 };
                    unsigned char hid_init[] = { 0xa0, 0, 0 };
                    hid_write( m_Device,  hid_init, 3 );
                    const int TOTAL_HID_BUTTONS = 69;
                    unsigned char button_hid_data[ 1 + TOTAL_HID_BUTTONS ];
                    memset(button_hid_data + 1,LED_ON,TOTAL_HID_BUTTONS);
                    button_hid_data[0] = 0x80;
                    button_hid_data[1] = LED_ON;
                    button_hid_data[2] = LED_ON;
                    button_hid_data[3] = LED_BRIGHT;
                    auto result = hid_write( m_Device,  button_hid_data, 1 + TOTAL_HID_BUTTONS );
                    
                    hid_set_nonblocking( m_Device, 1 );

                }
                else
                {
                    //std::cout << "Failed to open kontrol" << std::endl;
                }
            }
        }
        void TryConnectSometimes()
        {
            if(!Connected())
            {
                auto now = std::chrono::system_clock::now();
                int delay_ms = 1000;
                if(std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LastConnectTime).count() > delay_ms)
                {
                    m_LastConnectTime = now;
                    Connect();
                }
            }
        }
        void Disconnect()
        {
            if(m_Device)
            {
                hid_close(m_Device);
                m_Device = nullptr;
            }
        }
        void Run()
        {
            TryConnectSometimes();
            if(Connected())
            {
                std::vector<unsigned char> buf(1000);;
                int result = hid_read(m_Device, buf.data(), buf.size() - 1);
                if(result > 0)
                {
                    std::cout << "Read " << result << " bytes" << std::endl;
                    for(int i = 0; i < result; ++i)
                    {
                        std::cout << std::hex << (int)buf[i] << " ";
                    }
                    std::cout << std::endl;                    
                }
                else if(result < 0)
                {
                    std::cout << "Error reading from kontrol" << std::endl;
                    m_Device = nullptr;
                }
            }
        }
        ~Hid()
        {
            Disconnect();
            hid_exit();
        }
        bool Connected() const
        {
            return m_Device != nullptr;
        }
    private:
        hid_device* m_Device = nullptr;
        std::chrono::time_point<std::chrono::system_clock> m_LastConnectTime = std::chrono::system_clock::now();
    };

    class Display
    {
    public:
        Display()
        {
            std::fill(m_DisplayBuffer.begin(), m_DisplayBuffer.end(), 75);
            auto errcode = libusb_init_context(&m_Context, nullptr, 0);
            if(errcode)
            {
                throw std::runtime_error("Failed to init libusb");
            }
        }
        void Connect()
        {
            if(!Connected())
            {
                try
                {
                    uint16_t vid = 0x17cc;
                    uint16_t pid = 0x1620;
                    m_Device = libusb_open_device_with_vid_pid(m_Context, vid, pid);
                    if(m_Device)
                    {
                        auto errcode = libusb_claim_interface(m_Device, sInterfaceNumber);
                        if(errcode)
                        {
                            throw std::runtime_error("Failed to claim interface");
                        }
                    }
                }
                catch(...)
                {
                    libusb_close(m_Device);
                    m_Device = nullptr;
                }
                for(int x=0; x < 256; ++x)
                {
                    for(int y=0; y < 256; ++y)
                    {
                        m_DisplayBuffer[y * sDisplayBufferStride + x * sBytesPerPixel] = x;
                        m_DisplayBuffer[y * sDisplayBufferStride + x * sBytesPerPixel + 1] = y;
                    }
                }
                SendPixels(0, 0, sWidth, sHeight);
                //SendPixels(0, 300, 480, 20);
                //SendPixels(400, 48, 56, 32);
                
            }
        }
        void Disconnect()
        {
            if(Connected())
            {
                libusb_release_interface(m_Device, sInterfaceNumber);   
                libusb_close(m_Device);
                m_Device = nullptr;
            }
        }
        void SendPixels(int x, int y, int width, int height)
        {
            // https://github.com/GoaSkin/qKontrol/blob/b478fd9818c1b01c695722762e6d55a9b2e0228e/source/qkontrol.cpp#L150
            struct __attribute((packed)) Header
            {
                // all values are in big endian order!
                uint16_t c1 = 0x84;    // 84 00
                uint8_t screenindex;
                uint8_t c2 = 0x60;
                uint32_t c3 = 0;
                uint16_t x;
                uint16_t y;
                uint16_t width;
                uint16_t height;
                uint16_t c4 = 0x2;    // 02 00
                uint16_t c5 = 0;
                uint32_t datasizediv4;  
            };
            struct __attribute((packed)) Footer
            {
                uint32_t c1 = 2;    // 02 00 00 00
                uint32_t c2 = 3;    // 03 00 00 00
                uint32_t c3 = 0x40; // 40 00 00 00
            };
            

            int bottom = y + height;
            y = std::max(0, y);
            bottom = std::min(sHeight, bottom);
            height = bottom - y;
            int right = x + width;
            for(int displayindex: {0,1})
            {
                int displayleft = displayindex * sWidth / 2;
                int displayright = (displayindex + 1) * sWidth / 2;
                int disp_x = std::max(displayleft, x);
                int disp_right = std::min(displayright, right);

                int disp_width = disp_right - disp_x;
                long long datasizediv4 = ((long long)height * disp_width * sBytesPerPixel + 3) / 4;
                if(datasizediv4 > 0)
                {
                    size_t bufsize = sizeof(Header) + sizeof(Footer) + datasizediv4 * 4;
                    std::vector<unsigned char> buf(bufsize);
                    Header& header = *((Header*)buf.data());
                    header = Header();
                    std::span<unsigned char> data(buf.data() + sizeof(Header), datasizediv4 * 4);
                    Footer& footer = *((Footer*)(buf.data() + sizeof(Header) + datasizediv4 * 4));
                    footer = Footer();
                    header.screenindex = (uint8_t)displayindex;
                    header.x = std::byteswap((uint16_t)(disp_x- displayleft));
                    header.y = std::byteswap((uint16_t)y);
                    header.width = std::byteswap((uint16_t)disp_width);
                    header.height = std::byteswap((uint16_t)height);
                    header.datasizediv4 = std::byteswap((uint32_t)datasizediv4);
                    for(int yInBuf = 0; yInBuf < height; ++yInBuf)
                    {
                        const unsigned char *srcscanline = m_DisplayBuffer.data() + (y + yInBuf) * sDisplayBufferStride + disp_x * sBytesPerPixel;
                        unsigned char *destscanline = data.data() + yInBuf * disp_width * sBytesPerPixel;
                        std::copy(srcscanline, srcscanline + disp_width * sBytesPerPixel, destscanline);
                    }
                    auto errcode = libusb_bulk_transfer(m_Device, 0x03, buf.data(), (int)buf.size(), nullptr, 0);
                    if(errcode == LIBUSB_ERROR_NO_DEVICE )
                    {
                        Disconnect();
                    }
                }
            }

        }
        ~Display()
        {
            Disconnect();
            if(m_Context)
            {
                libusb_exit(m_Context);
                m_Context = nullptr;
            }
        }
        void TryConnectSometimes()
        {
            if(!Connected())
            {
                auto now = std::chrono::system_clock::now();
                int delay_ms = 1000;
                if(std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LastConnectTime).count() > delay_ms)
                {
                    m_LastConnectTime = now;
                    Connect();
                }
            }
        }
        bool Connected() const
        {
            return m_Device != nullptr;
        }
        void Run()
        {
            TryConnectSometimes();
            if(Connected())
            {
            }
        }
        void Test()
        {
            if(Connected())
            {
                int x = 0;
                int y = 0;
                int width = 0;
                int height = 0;
                unsigned short color = 0;
                std::span<unsigned short> pixels((unsigned short*)m_DisplayBuffer.data(), m_DisplayBuffer.size() / 2);
                std::fill(pixels.begin(), pixels.end(), color);
                SendPixels(x, y, width, height);
            }
        }
    private:
        static constexpr int sWidth = 480 * 2;
        static constexpr int sHeight = 272;
        static constexpr int sBytesPerPixel = 2;
        static constexpr int sDisplayBufferStride = sBytesPerPixel * sWidth;
        std::vector<unsigned char> m_DisplayBuffer = std::vector<unsigned char>((size_t)sHeight*sDisplayBufferStride);
        static constexpr int sInterfaceNumber = 3;
     	libusb_context *m_Context = nullptr;
        libusb_device_handle* m_Device = nullptr;
        std::chrono::time_point<std::chrono::system_clock> m_LastConnectTime = std::chrono::system_clock::now();
    };
}