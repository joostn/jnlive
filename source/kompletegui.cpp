#include "komplete.h"
#include <thread>
#include "kompletegui.h"
#include "simplegui.h"

using std::chrono_literals::operator""ms;

namespace komplete
{
    Gui::Gui(std::pair<int, int> vidPid) : m_Hid(vidPid, [this](Hid::TButtonIndex button, int delta) { OnButton(button, delta); })
    {
        m_GuiThread = std::thread([vidPid, this]() {
            RunGuiThread(vidPid);
        });
    }

    void Gui::OnButton(Hid::TButtonIndex button, int delta)
    {

    }

    Gui::~Gui()
    {
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_AbortRequested = true;
        }
        m_GuiThread.join();
    }

    void Gui::Run()
    {
        m_Hid.Run();
    }

    void Gui::RunGuiThread(std::pair<int, int> vidPid)
    {
        Display display(vidPid);
        while(true)
        {
            std::unique_ptr<simplegui::Window> window;
            {
                std::unique_lock<std::mutex> lock(m_Mutex);
                if(m_AbortRequested)
                {
                    return;
                }
                window = std::move(m_NewWindow);
            }
            if(window)
            {
                // todo: find dirty regions
                PaintWindow(display, *window);
                m_PrevWindow = std::move(window);
                display.SendPixels(0, 0, Display::sWidth, Display::sHeight);
            }
            display.Run();
            std::this_thread::sleep_for(100ms);
        }
    }

    void Gui::SetWindow(std::unique_ptr<simplegui::Window> window)
    {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_NewWindow = std::move(window);
    }

    void Gui::PaintWindow(Display &display, const simplegui::Window &window)
    {
        uint16_t* pixelbuf = display.DisplayBuffer(0, 0);
        auto surface = Cairo::ImageSurface::create((unsigned char*)pixelbuf, Cairo::Format::FORMAT_RGB16_565, Display::sWidth, Display::sHeight, Display::sDisplayBufferStride);
        auto cr = Cairo::Context::create(surface);
        // fill black:
        cr->set_source_rgb(0, 0, 0);
        cr->paint();
        Cairo::Context *context = cr.operator->();
        window.Paint(*context);
    }

}
