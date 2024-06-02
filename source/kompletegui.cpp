#include "komplete.h"
#include <thread>
#include "kompletegui.h"
#include "simplegui.h"

using std::chrono_literals::operator""ms;

namespace komplete
{
    Gui::Gui(std::pair<int, int> vidPid, engine::Engine &engine) : m_Engine(engine), m_Hid(vidPid, [this](Hid::TButtonIndex button, int delta) { OnButton(button, delta); })
    {
        m_GuiThread = std::thread([vidPid, this]() {
            RunGuiThread(vidPid);
        });
    }

    void Gui::OnButton(Hid::TButtonIndex button, int delta)
    {
        if(m_GuiState.m_Mode == TGuiState::TMode::Performance)
        {
            if(button == Hid::TButtonIndex::LcdRotary0)
            {
                auto v = m_GuiState.m_SelectedPresetHysteresis.Update(delta);
                if(v != 0)
                {
                    m_GuiState.m_SelectedPreset += v;
                    const auto &project = m_Engine.Project();
                    m_GuiState.m_SelectedPreset = std::clamp(m_GuiState.m_SelectedPreset, 0, (int)project.QuickPresets().size() - 1);
                    Refresh();
                }
            }
            if( (button == Hid::TButtonIndex::Shift) && (delta > 0) )
            {
                m_GuiState.m_Shift = !m_GuiState.m_Shift;
                RefreshLeds();
                Refresh();
            }
            if( (button == Hid::TButtonIndex::Instance) && (delta > 0) )
            {
                const auto &project = m_Engine.Project();
                std::optional<size_t> newfocusedpart;
                if(!project.Parts().empty())
                {
                    size_t newfocusedpart = 0;
                    if(project.FocusedPart())
                    {
                        newfocusedpart = project.FocusedPart().value() + 1;
                        if(newfocusedpart >= project.Parts().size())
                        {
                            newfocusedpart = 0;
                        }
                        if(newfocusedpart != project.FocusedPart().value())
                        {
                            auto newproject = project.Change(newfocusedpart, project.ShowUi());
                            m_Engine.SetProject(std::move(newproject));
                        }
                    }
                }
            }
        }

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
        Refresh();
        RefreshLeds();
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
    void Gui::OnProjectChanged()
    {
        const auto &project = m_Engine.Project();
        m_GuiState.m_SelectedPreset = std::clamp(m_GuiState.m_SelectedPreset, 0, (int)project.QuickPresets().size() - 1);
        Refresh();
    }
    void Gui::Refresh()
    {
        auto mainwindow = std::make_unique<simplegui::Window>(nullptr, Gdk::Rectangle(0, 0, Display::sWidth, Display::sWidth));
        int fontsize = 15;
        int lineheight = fontsize + 2;
        int linespacing = 2;

        const auto &project = m_Engine.Project();
        std::string presetname;
        if(m_GuiState.m_SelectedPreset < (int)project.QuickPresets().size())
        {
            const auto &presetOrNull = project.QuickPresets()[m_GuiState.m_SelectedPreset];
            if(presetOrNull)
            {
                presetname = presetOrNull->Name();
            }
        }
        int presetboxheight = 2*lineheight + linespacing + 2;
        auto presetnamebox = mainwindow->AddChild<simplegui::PlainWindow>(Gdk::Rectangle(0, 272-presetboxheight, 120, presetboxheight), simplegui::Rgba(0.2, 0.2, 0.2));

        presetnamebox->AddChild<simplegui::TextWindow>(Gdk::Rectangle(1, 1, presetnamebox->Width() - 2, lineheight), std::to_string(m_GuiState.m_SelectedPreset), simplegui::Rgba(1, 1, 1), fontsize);

        presetnamebox->AddChild<simplegui::TextWindow>(Gdk::Rectangle(1, 1 + lineheight + linespacing, presetnamebox->Width() - 2, lineheight), presetname, simplegui::Rgba(1, 1, 1), fontsize);

        if(project.FocusedPart())
        {
            int boxwidth = 100;
            int boxheight = 40;
            simplegui::Rgba boxcolor = (project.FocusedPart().value() == 0)? simplegui::Rgba(1, 0.5, 0.5): simplegui::Rgba(0.5, 1, 0.5);
            auto box = mainwindow->AddChild<simplegui::PlainWindow>(Gdk::Rectangle(960-boxwidth, (272-boxheight)/2, boxwidth, boxheight), boxcolor);
            box->AddChild<simplegui::TextWindow>(Gdk::Rectangle(3, 3, boxwidth - 6, boxheight - 6), project.Parts().at(project.FocusedPart().value()).Name(), simplegui::Rgba(0, 0, 0), 25);

        }

        SetWindow(std::move(mainwindow));
    }
    void Gui::RefreshLeds()
    {
        m_Hid.SetLed(Hid::TButtonIndex::Shift, m_GuiState.m_Shift? 4:1);
        m_Hid.SetLed(Hid::TButtonIndex::Instance, 1);
    }
}
