#pragma once

#include <hidapi.h>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <cstring>

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
            m_Inited = true;
            m_Device = hid_open(0x17cc, 0x1620, nullptr);
            if(m_Device)
            {
                hid_set_nonblocking( m_Device, 0 );

                unsigned char hid_packet_interactive[] = { 0xa0, 0x03, 0x04 };
                unsigned char hid_packet_midi_mode[] = { 0xa0, 0x07, 0x00 };
                hid_write( m_Device,  hid_packet_midi_mode, 3 );
                const int TOTAL_HID_BUTTONS = 21;
                unsigned char button_hid_data[ 1 + TOTAL_HID_BUTTONS ];
                memset(button_hid_data + 1,0,TOTAL_HID_BUTTONS);
                button_hid_data[0] = 0x80;
#define LED_BRIGHT          0x7e
#define LED_ON              0x7c
#define LED_OFF             0x00
                button_hid_data[1] = LED_ON;
                button_hid_data[2] = LED_ON;
                button_hid_data[3] = LED_BRIGHT;
                auto result = hid_write( m_Device,  button_hid_data, 1 + TOTAL_HID_BUTTONS );
                
                hid_set_nonblocking( m_Device, 1 );

            }
            else
            {
                std::cout << "Failed to open kontrol" << std::endl;
            }
        }
        void Run()
        {
            if(m_Device)
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
            if(m_Device)
            {
                hid_close(m_Device);
            }
            if(m_Inited)
            {
                hid_exit();
            }
        }
    private:
        bool m_Inited = false;
        hid_device* m_Device = nullptr;

    };
}