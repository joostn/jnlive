#include "komplete.h"
#include <hidapi.h>
#include <libusb.h>
#include <stdexcept>
#include <span>
#include <bit>

namespace {
    std::string get_string_descriptor_utf8(libusb_device_handle *dev,
	uint8_t desc_index)
    {
        std::vector<unsigned char> tbuf(255);
        int  si, di;
        /* Get all language ids. */
        auto r = libusb_get_string_descriptor(dev, 0, 0, tbuf.data(), tbuf.size());
        if (r < 4)
        {
            throw std::runtime_error("Failed to get language ids");
        }
        // get first language id:
        int langid = tbuf[2] | (tbuf[3] << 8);
        r = libusb_get_string_descriptor(dev, desc_index, langid, tbuf.data(), tbuf.size());
        if( (r < 0) || (tbuf[1] != LIBUSB_DT_STRING) || (tbuf[0] > r) || (tbuf[0] < 2) || (r >= tbuf.size()) )
        {
            throw std::runtime_error("Failed to get string descriptor");
        }
        auto len = (size_t)(tbuf[0] - 2)/2;
        std::wstring wresult;
        wresult.reserve(len);
        for(size_t i=0; i < len; i++)
        {
            wchar_t c = tbuf[2*i+2] | ((wchar_t)tbuf[2*i+3] << 8);
            wresult.push_back(c);
        }
        return utils::wu8(wresult);
    }

}
namespace komplete
{
    Hid::Hid(std::pair<int, int> vidPid, std::string_view serial, TOnButton &&onButton) : m_OnButton(std::move(onButton))
    {
        auto err = hid_init();
        if(err)
        {
            throw std::runtime_error("Failed to init hidapi");
        }
        auto wserial = utils::u8w(serial);
        auto device = hid_open(vidPid.first, vidPid.second, wserial.c_str());
        utils::finally finally([&](){
            if(device) hid_close(device);
            device = nullptr;
        });
        if(!device)
        {
            throw std::runtime_error("Failed to open device");
        }
        auto r = hid_set_nonblocking( device, 0 );
        if(r < 0)
        {
            throw std::runtime_error("Failed to set nonblocking mode");
        }
/*
        {
            unsigned char hid_init[] = { 0xa0, 0x93, 0 };
            r = hid_write( device,  hid_init, 3 );
            if(r != 3)
            {
                throw std::runtime_error("Failed to write to device");
            }
        }

        {
            unsigned char hid_init[] = { 0xaf, 0, 2 };
            r = hid_write( device,  hid_init, 3 );
            if(r != 3)
            {
                throw std::runtime_error("Failed to write to device");
            }
        }
        {
            unsigned char hid_init[] = { 0xa0, 0, 0 };
            r = hid_write( device,  hid_init, 3 );
            if(r != 3)
            {
                throw std::runtime_error("Failed to write to device");
            }
        }
*/

        {
            unsigned char hid_init[] = { 0xa0, 0x00, 0x10 };
            r = hid_write( device,  hid_init, 3 );
            if(r != 3)
            {
                throw std::runtime_error("Failed to write to device");
            }
        }

        r = hid_set_nonblocking( device, 1 );
        if(r < 0)
        {
            throw std::runtime_error("Failed to set nonblocking mode");
        }
        m_Device = device;
        device = nullptr;
        m_LedStateChanged = true;
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
    void Hid::Disconnect()
    {
        if(m_Device)
        {
            hid_close(m_Device);
            m_Device = nullptr;
            UpdateButtonState({});  // send button up events
        }
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
    void Hid::Run()
    {
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
                else if(reportid == 2)
                {
                    // touch strip:
                    if(numbytes >= 9)
                    {
                        auto value = *((int*)&buf[5]);
                        value -= 100000;
                        if(value < 0)
                        {
                            std::cout << " End touch" << std::endl;
                        }
                        else
                        {
                            std::cout << " Touch: " << std::dec << value << std::endl;
                        }
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
    }

    /////////////
    Display::~Display()
    {
        Disconnect();
    }
    
    Display::Display(std::pair<int, int> vidPid, std::string_view serial) 
    {
     	libusb_context *context = nullptr;
        libusb_device_handle* device = nullptr;
        libusb_device **devs = nullptr;
        utils::finally finally([&](){
            if(devs) libusb_free_device_list(devs, 1);
            devs = nullptr;
            if(device) libusb_close(device);
            device = nullptr;
            if(context) libusb_exit(context);
            context = nullptr;
        });
        int errcode = libusb_init_context(&context, nullptr, 0);
        if(errcode)
        {
            throw std::runtime_error("Failed to init libusb");
        }
        // Get the list of USB devices
        auto count = libusb_get_device_list(context, &devs);
        if (count < 0) 
        {
            throw std::runtime_error("libusb_get_device_list failed");
        }
        for (size_t i = 0; i < (size_t)count; i++) 
        {
            auto trydevice = devs[i];
            struct libusb_device_descriptor desc;
            auto r = libusb_get_device_descriptor(trydevice, &desc);
            if (r == LIBUSB_SUCCESS)
            {
                if (desc.idVendor == vidPid.first && desc.idProduct == vidPid.second) 
                {
                    // Open the device
                    r = libusb_open(trydevice, &device);
                    if (r == LIBUSB_SUCCESS)
                    {
                        auto thisserial = get_string_descriptor_utf8(device, desc.iSerialNumber);
                        if(thisserial == serial)
                        {
                            // found it, and opened successfully:
                            goto success;
                        }
                    }
                    if(device) libusb_close(device);
                    device = nullptr;
                }
            }
        } // for device
        throw std::runtime_error("Failed to find device");

    success:
        errcode = libusb_claim_interface(device, sInterfaceNumber);
        if(errcode)
        {
            throw std::runtime_error("Failed to claim interface");
        }
        m_Context = context;
        context = nullptr;
        m_Device = device;
        device = nullptr;
        std::fill(m_DisplayBuffer.begin(), m_DisplayBuffer.end(), 0x00);
        // SendPixels(utils::TIntRect::FromSize({sWidth, sHeight}));
    }
    void Display::Disconnect()
    {
        if(Connected())
        {
            libusb_release_interface(m_Device, sInterfaceNumber);   
            libusb_close(m_Device);
            m_Device = nullptr;
        }
        if(m_Context) libusb_exit(m_Context);
        m_Context = nullptr;
    }
    void Display::SendPixels(const utils::TIntRegion &region)
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
                uint16_t x = 0;
                uint16_t y = 0;
                uint16_t hstride;
                uint16_t unknown1;
            };
            struct __attribute((packed)) LineHeader
            {
                uint16_t c1 = 0x02;    // 02 00
                uint16_t offset = 0;
                uint16_t c2 = 0;
                uint16_t numwords = 0;
            };
            struct __attribute((packed)) Footer
            {
                uint32_t c1 = 2;    // 02 00 00 00
                uint32_t c2 = 3;    // 03 00 00 00
                uint32_t c3 = 0x40; // 40 00 00 00
            };
            for(size_t displayindex: {0,1})
            {
                auto displayrect = utils::TIntRect::FromTopLeftAndSize({(int)displayindex * sWidth / 2, 0}, {sWidth / 2, sHeight});
                auto regionfordisplay = region.Intersection(displayrect);
                size_t pixeloffset_int = 0; // offset in 4 byte dwords in device's pixel buffer
                // each pixel occupies 16 bits, so 2 pixels per uint32_t
                if(!regionfordisplay.empty())
                {
                    std::vector<unsigned char> buf;
                    Header header;
                    header.screenindex = (uint8_t)displayindex;
                    header.hstride = std::byteswap((uint16_t)480);
                    header.unknown1 = std::byteswap((uint16_t)1);
                    std::copy((unsigned char*)&header, (unsigned char*)&header + sizeof(header), std::back_inserter(buf));
                    for(const auto &vrange: regionfordisplay.VertRanges())
                    {
                        // we can only start and end at even pixel coordinates:
                        nhAssert(vrange.Top() >= 0);
                        nhAssert(vrange.Bottom() <= sHeight);
                        utils::TIntRegion::TVerticalRange modifiedverticalrange(vrange.Top(), vrange.Bottom());
                        for(const auto &hrange: vrange.HorzRanges())
                        {
                            int left = hrange.Left() & (~1);
                            int right = (hrange.Right() + 1) & (~1);
                            nhAssert(left >= displayrect.Left());
                            nhAssert(right <= displayrect.Left() + sWidth / 2);
                            modifiedverticalrange.AppendHorzRange({left, right});
                        }
                        for(int y = modifiedverticalrange.Top(); y < modifiedverticalrange.Bottom(); y++)
                        {
                            for(const auto &hrange: modifiedverticalrange.HorzRanges())
                            {
                                size_t numwords = (size_t)(hrange.Right() - hrange.Left()) / 2;
                                size_t startwordindex = (y * sWidth / 2 + hrange.Left() - displayrect.Left()) / 2;
                                nhAssert(startwordindex >= pixeloffset_int);
                                auto skipwords = startwordindex - pixeloffset_int;
                                nhAssert(skipwords <= std::numeric_limits<uint16_t>::max());
                                nhAssert(numwords <= std::numeric_limits<uint16_t>::max());
                                LineHeader lineheader;
                                lineheader.numwords = std::byteswap((uint16_t)numwords);
                                lineheader.offset = std::byteswap((uint16_t)skipwords);
                                pixeloffset_int += numwords + skipwords;
                                std::copy((unsigned char*)&lineheader, (unsigned char*)&lineheader + sizeof(lineheader), std::back_inserter(buf));
                                const unsigned char *srcscanline = m_DisplayBuffer.data() + y * sDisplayBufferStride;
                                for(int xInBuf = hrange.Left(); xInBuf < hrange.Right(); ++xInBuf)
                                {
                                    buf.push_back(srcscanline[xInBuf * sBytesPerPixel + 1]);
                                    buf.push_back(srcscanline[xInBuf * sBytesPerPixel + 0]);
                                }
                            }
                        }
                    } // for vrange
                    Footer footer;
                    std::copy((unsigned char*)&footer, (unsigned char*)&footer + sizeof(footer), std::back_inserter(buf));
                    m_LastPing = std::chrono::system_clock::now();
                    auto errcode = libusb_bulk_transfer(m_Device, 0x03, buf.data(), (int)buf.size(), nullptr, 0);
                    if(errcode == LIBUSB_ERROR_NO_DEVICE )
                    {
                        Disconnect();
                        return;
                    }
                } // if (!empty)
            } // for displayindex
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
            int delay_ms = 100;
            if(std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LastPing).count() > delay_ms)
            {
                SendPixels(utils::TIntRect::FromSize({2}));
            }
        }
    }

}