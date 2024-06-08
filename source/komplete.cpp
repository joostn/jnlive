#include "komplete.h"
#include <hidapi.h>
#include <libusb.h>
#include <stdexcept>

namespace komplete
{
    Hid::Hid(std::pair<int, int> vidPid, TOnButton &&onButton) : m_VidPid(vidPid), m_OnButton(std::move(onButton))
    {
        auto err = hid_init();
        if(err)
        {
            throw std::runtime_error("Failed to init hidapi");
        }
        std::fill(std::begin(m_LedState), std::end(m_LedState), 0);
        m_LedMap.resize(72, -1);
        m_LedMap[(size_t)TButtonIndex::Mute] = 0;
        m_LedMap[(size_t)TButtonIndex::Solo] = 1;
        m_LedMap[(size_t)TButtonIndex::Menu0] = 2;
        m_LedMap[(size_t)TButtonIndex::Menu1] = 3;
        m_LedMap[(size_t)TButtonIndex::Menu2] = 4;
        m_LedMap[(size_t)TButtonIndex::Menu3] = 5;
        m_LedMap[(size_t)TButtonIndex::Menu4] = 6;
        m_LedMap[(size_t)TButtonIndex::Menu5] = 7;
        m_LedMap[(size_t)TButtonIndex::Menu6] = 8;
        m_LedMap[(size_t)TButtonIndex::Menu7] = 9;
        m_LedMap[(size_t)TButtonIndex::RotaryLeft] = 10;
        m_LedMap[(size_t)TButtonIndex::RotaryUp] = 11;
        m_LedMap[(size_t)TButtonIndex::RotaryDown] = 12;
        m_LedMap[(size_t)TButtonIndex::RotaryRight] = 13;
        m_LedMap[(size_t)TButtonIndex::Shift] = 14;
        m_LedMap[(size_t)TButtonIndex::Scale] = 15;
        m_LedMap[(size_t)TButtonIndex::Arp] = 16;
        m_LedMap[(size_t)TButtonIndex::Scene] = 17;
        m_LedMap[(size_t)TButtonIndex::Undo] = 18;
        m_LedMap[(size_t)TButtonIndex::Quantize] = 19;
        m_LedMap[(size_t)TButtonIndex::Auto] = 20;
        m_LedMap[(size_t)TButtonIndex::Pattern] = 21;
        m_LedMap[(size_t)TButtonIndex::PresetUp] = 22;
        m_LedMap[(size_t)TButtonIndex::Track] = 23;
        m_LedMap[(size_t)TButtonIndex::Loop] = 24;
        m_LedMap[(size_t)TButtonIndex::Metro] = 25;
        m_LedMap[(size_t)TButtonIndex::Tempo] = 26;
        m_LedMap[(size_t)TButtonIndex::PresetDown] = 27;
        m_LedMap[(size_t)TButtonIndex::KeyMode] = 28;
        m_LedMap[(size_t)TButtonIndex::Play] = 29;
        m_LedMap[(size_t)TButtonIndex::Rec] = 30;
        m_LedMap[(size_t)TButtonIndex::Stop] = 31;
        m_LedMap[(size_t)TButtonIndex::Left] = 32;
        m_LedMap[(size_t)TButtonIndex::Right] = 33;
        m_LedMap[(size_t)TButtonIndex::Clear] = 34;
        m_LedMap[(size_t)TButtonIndex::Browser] = 35;
        m_LedMap[(size_t)TButtonIndex::Plugin] = 36;
        m_LedMap[(size_t)TButtonIndex::Mixer] = 37;
        m_LedMap[(size_t)TButtonIndex::Instance] = 38;
        m_LedMap[(size_t)TButtonIndex::Midi] = 39;
        m_LedMap[(size_t)TButtonIndex::Setup] = 40;
        m_LedMap[(size_t)TButtonIndex::Fixed] = 41;
        // 42, 43 = octave?
        // 44..68 = touch strip
    }
    void Hid::ClearAllLeds()
    {
        std::fill(std::begin(m_LedState), std::end(m_LedState), 0);
        m_LedStateChanged = true;        
    }
    void Hid::SetLed(TButtonIndex button, TLedColor color, int brightness)
    {
        brightness = std::clamp(brightness, 0, 4);
        if(brightness == 0)
        {
            color = TLedColor::Off;
            brightness = 1;
        }
        auto b = (size_t)button;
        int ledindex = -1;
        if(b < m_LedMap.size())
        {
            ledindex = m_LedMap[b];
        }
        if(ledindex >= 0)
        {
            int value = (int)color * 4 + brightness - 1;
            if(m_LedState[ledindex] != value)
            {
                m_LedState[ledindex] = value;
                m_LedStateChanged = true;
            }
        }
    }
    void Hid::Connect()
    {
        if(!m_Device)
        {
            m_Device = hid_open(m_VidPid.first, m_VidPid.second, nullptr);
            if(m_Device)
            {
                hid_set_nonblocking( m_Device, 0 );
                unsigned char hid_init[] = { 0xa0, 0, 0 };
                hid_write( m_Device,  hid_init, 3 );
                hid_set_nonblocking( m_Device, 1 );
                m_LedStateChanged = true;
            }
        }
    }
    void Hid::UpdateLedState()
    {
        if(Connected())
        {
            std::vector<unsigned char> buf;
            buf.reserve(70);
            buf.push_back(0x80);
            std::copy(std::begin(m_LedState), std::end(m_LedState), std::back_inserter(buf));
            auto result = hid_write( m_Device, buf.data(), buf.size());
            if(result < 0)
            {
                // usb error:
                Disconnect();
            }
        }
    }
    void Hid::TryConnectSometimes()
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
    void Hid::Disconnect()
    {
        if(m_Device)
        {
            hid_close(m_Device);
            m_Device = nullptr;
            UpdateButtonState({});  // send button up events
        }
    }
    void Hid::Run()
    {
        TryConnectSometimes();
        if(Connected())
        {
            std::vector<unsigned char> buf(1000);;
            int numbytes = hid_read(m_Device, buf.data(), buf.size() - 1);
            if(numbytes > 0)
            {
                int reportid = buf[0];
                if(reportid == 0x01)
                {
                    if(numbytes >= 31)
                    {
                        // buttons and dials:
                        std::vector<int> buttonstate;
                        for(int i=0; i < 72; i++)
                        {
                            // 72 * 1 bit
                            // menu buttons: fix ordering:
                            auto i1 = i;
                            if(i1 < 8)
                            {
                                i1 ^= 3;
                            }
                            auto b = buf[(i1 >> 3) + 1];
                            auto mask = 128 >> (i1 & 7);
                            buttonstate.push_back((b & mask) != 0? 1:0);
                        }
                        for(int i=0; i < 10; i++)
                        {
                            // 8 x 2 bytes (0-999)
                            // 2 x 2 bytes (0-1023)
                            buttonstate.push_back(*((const uint16_t*)&buf[(i * 2) + 10]));
                        }
                        // 2 x 4 bits:
                        buttonstate.push_back(buf[30] & 0x0f);
                        buttonstate.push_back((buf[30] >> 4) & 0x0f);
                        // 1 x 1 byte (0-255):
                        buttonstate.push_back(buf[31]);
                        UpdateButtonState(std::move(buttonstate));
                    }
 
                }
                else
                {
                    // std::cout << "Read " << numbytes << " bytes" << std::endl;
                    // for(int i = 0; i < numbytes; ++i)
                    // {
                    //     std::cout << std::hex << (int)buf[i] << " ";
                    // }
                    // std::cout << std::dec << std::endl;
                }
            }
            else if(numbytes < 0)
            {
                // usb error:
                Disconnect();
            }
            if(m_LedStateChanged)
            {
                m_LedStateChanged = false;
                UpdateLedState();
            }
        }
    }
    void Hid::UpdateButtonState(std::vector<int> &&buttonstate)
    {
        auto prevbuttonstate = std::move(m_PrevButtonState);
        m_PrevButtonState = std::move(buttonstate);
        const auto &newbuttonstate = m_PrevButtonState;
        if(newbuttonstate.empty() && (!prevbuttonstate.empty()))
        {
            // send button up events
            for(int i=0; i < std::min(72, (int)prevbuttonstate.size()); i++)
            {
                if(prevbuttonstate[i] != 0)
                {
                    SendButtonDelta((TButtonIndex)i, -prevbuttonstate[i]);
                }
            }
        }
        else if( (!newbuttonstate.empty()) && (prevbuttonstate.empty()))
        {
            // send button down events
            for(int i=0; i < std::min(72, (int)newbuttonstate.size()); i++)
            {
                if(newbuttonstate[i] != 0)
                {
                    SendButtonDelta((TButtonIndex)i, newbuttonstate[i]);
                }
            }
        }
        else if( (!newbuttonstate.empty()) && (!prevbuttonstate.empty()))
        {
            for(int i = 0; i < std::min((int)prevbuttonstate.size(), (int)newbuttonstate.size()); i++)
            {
                if(newbuttonstate[i] != prevbuttonstate[i])
                {
                    int wrapvalue = (i < 72)? 9 : ((i < 80)? 1000 : (i < 82)? 1024 : ((i < 84)? 16 : 256));
                    int delta = newbuttonstate[i] - prevbuttonstate[i];
                    if(delta > wrapvalue / 2)
                    {
                        delta -= wrapvalue;
                    }
                    else if(delta < -wrapvalue / 2)
                    {
                        delta += wrapvalue;
                    }
                    SendButtonDelta((TButtonIndex)i, delta);
                }
            }
        }
    }
    int Hid::GetButtonState(TButtonIndex index) const
    {
        auto index_int = (size_t)index;
        if(index_int < m_PrevButtonState.size())
        {
            return m_PrevButtonState[index_int];
        }
        return 0;
    }
    void Hid::SendButtonDelta(TButtonIndex button, int delta)
    {
        if(m_OnButton)
        {
            m_OnButton(button, delta);
        }

    }
    Hid::~Hid()
    {
        Disconnect();
        hid_exit();
    }
    /////////////
    Display::Display(std::pair<int, int> vidPid) : m_VidPid(vidPid)
    {
        std::fill(m_DisplayBuffer.begin(), m_DisplayBuffer.end(), 0xff);
        auto errcode = libusb_init_context(&m_Context, nullptr, 0);
        if(errcode)
        {
            throw std::runtime_error("Failed to init libusb");
        }
    }
    void Display::Connect()
    {
        if(!Connected())
        {
            try
            {
                m_Device = libusb_open_device_with_vid_pid(m_Context, m_VidPid.first, m_VidPid.second);
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
            SendPixels(0, 0, sWidth, sHeight);
        }
    }
    void Display::Disconnect()
    {
        if(Connected())
        {
            libusb_release_interface(m_Device, sInterfaceNumber);   
            libusb_close(m_Device);
            m_Device = nullptr;
        }
    }
    void Display::SendPixels(int x, int y, int width, int height)
    {        
        // https://github.com/GoaSkin/qKontrol/blob/b478fd9818c1b01c695722762e6d55a9b2e0228e/source/qkontrol.cpp#L150
        if(Connected())
        {
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
            int right = x + width;
            x = x & (~1);
            y = y & (~1);
            right = (right + 1) & (~1);
            bottom = (bottom + 1) & (~1);
            y = std::max(0, y);
            bottom = std::min(sHeight, bottom);
            height = bottom - y;
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
                        for(int xInBuf = 0; xInBuf < disp_width; ++xInBuf)
                        {
                            destscanline[xInBuf * sBytesPerPixel + 0] = srcscanline[xInBuf * sBytesPerPixel + 1];
                            destscanline[xInBuf * sBytesPerPixel + 1] = srcscanline[xInBuf * sBytesPerPixel + 0];
                        }
                    }
                    m_LastPing = std::chrono::system_clock::now();
                    auto errcode = libusb_bulk_transfer(m_Device, 0x03, buf.data(), (int)buf.size(), nullptr, 0);
                    if(errcode == LIBUSB_ERROR_NO_DEVICE )
                    {
                        Disconnect();
                        return;
                    }
                }
            }
        }
    }
    Display::~Display()
    {
        Disconnect();
        if(m_Context)
        {
            libusb_exit(m_Context);
            m_Context = nullptr;
        }
    }
    void Display::TryConnectSometimes()
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
    void Display::PingSometimes()
    {
        // 'ping' the display by sending a small block of pixels.
        // SendPixels() will cause a Disconnect() if the device is no longer connected.
        // This will then start the reconnection attempts.
        auto now = std::chrono::system_clock::now();
        if(Connected())
        {
            int delay_ms = 1000;
            if(std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LastPing).count() > delay_ms)
            {
                SendPixels(0, 0, 16, 16);
            }
        }
    }
    void Display::Run()
    {
        TryConnectSometimes();
        PingSometimes();
    }

}