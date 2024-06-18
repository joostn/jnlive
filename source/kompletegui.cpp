#include "komplete.h"
#include <thread>
#include "kompletegui.h"
#include "simplegui.h"

using std::chrono_literals::operator""ms;

namespace komplete
{
    constexpr double sMinSliderValue = -60;
    constexpr double sMaxSliderValue = 10;
    constexpr double sSliderPow = 0.6;
    double multiplierToSliderValue(double multiplier)
    {
        if(multiplier <= 0.0) return 0.0;
        auto db = 20 * log10(multiplier);
        auto slidervalue = (db - sMinSliderValue) / (sMaxSliderValue - sMinSliderValue);
        slidervalue = std::clamp(slidervalue, 0.0, 1.0);
        slidervalue = std::pow(slidervalue, sSliderPow);
        return slidervalue;
    }

    double sliderValueToMultiplier(double slidervalue)
    {
        if(slidervalue <= 0) return 0.0;
        slidervalue = std::min(slidervalue, 1.0);
        slidervalue = std::pow(slidervalue, 1.0 / sSliderPow);
        auto db = slidervalue * (sMaxSliderValue - sMinSliderValue) + sMinSliderValue;
        return pow(10, db / 20);
    }

    double increaseMultiplier(double origvalue, double deltaDb)
    {   
        double db;
        if(origvalue <= 0.0)
        {
            db = sMinSliderValue;
        }
        else
        {
            db = 20 * log10(origvalue);
            db = std::max(db, sMinSliderValue);
        }
        db += deltaDb;
        if(db <= sMinSliderValue) return 0.0;
        db = std::min(db, sMaxSliderValue);
        return pow(10, db / 20);
    }

    simplegui::Rgba colorForPart(size_t partindex)
    {
        return (partindex == 0)? simplegui::Rgba(1, 0.3, 0.3): simplegui::Rgba(0.3, 1, 0.3);
    }

    std::string multiplierToText(double v)
    {
        double db;
        if(v <= 0.0)
        {
            db = sMinSliderValue;
        }
        else
        {
            db = 20 * log10(v);
            db = std::max(db, sMinSliderValue);
        }
        if(db <= sMinSliderValue) return "-inf";
        int dbRounded = (int)std::lround(db);
        if(dbRounded > 0)
        {
            return "+" + std::to_string(dbRounded);
        }
        return std::to_string(dbRounded);
    }


    Gui::Gui(std::pair<int, int> vidPid, engine::Engine &engine) : m_Engine(engine), m_Hid(vidPid, [this](Hid::TButtonIndex button, int delta) { OnButton(button, delta); })
    {
        m_GuiThread = std::thread([vidPid, this]() {
            RunGuiThread(vidPid);
        });
        Refresh();
        RefreshLeds();
    }

    void Gui::OnButton(Hid::TButtonIndex button, int delta)
    {
        const auto &project = m_Engine.Project();
        if( (button == Hid::TButtonIndex::Shift) && (delta > 0) )
        {
            m_GuiState.m_Shift = !m_GuiState.m_Shift;
        }
        if( (button == Hid::TButtonIndex::Browser) && (delta > 0) )
        {
            m_GuiState.m_Mode = TGuiState::TMode::Performance;
        }
        if( (button == Hid::TButtonIndex::Midi) && (delta > 0) )
        {
            m_GuiState.m_Mode = TGuiState::TMode::Midi;
        }
        if( (button == Hid::TButtonIndex::Arp) && (delta > 0) )
        {
            const auto &project = m_Engine.Project();
            if(!project.Parts().empty())
            {
                auto newproject = project.Change(project.FocusedPart(), !project.ShowUi());
                m_Engine.SetProject(std::move(newproject));
            }
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
        if(m_GuiState.m_Mode == TGuiState::TMode::Performance)
        {
            if(button == Hid::TButtonIndex::LcdRotary0)
            {
                auto v = m_GuiState.m_SelectedPresetHysteresis.Update(delta);
                if(!project.Presets().empty())
                {
                    if(v != 0)
                    {
                        m_GuiState.m_SelectedPreset = (size_t)std::clamp((int)m_GuiState.m_SelectedPreset + v, 0, (int)project.Presets().size() - 1);
                        Refresh();
                    }
                }
            }
            if( (button >= Hid::TButtonIndex::LcdRotary5) && (button <= Hid::TButtonIndex::LcdRotary7) )
            {
                utils::THysteresis &hysteresis = (button == Hid::TButtonIndex::LcdRotary5)? m_GuiState.m_ReverbLevelHysteresis : (button == Hid::TButtonIndex::LcdRotary6)? m_GuiState.m_Part1LevelHysteresis : m_GuiState.m_Part2LevelHysteresis;
                auto v = m_GuiState.m_SelectedPresetHysteresis.Update(delta);
                if(v != 0)
                {
                    double dB = v * 0.5;
                    if(button == Hid::TButtonIndex::LcdRotary5)
                    {
                        auto multiplier = project.Reverb().MixLevel();
                        auto newmultiplier = increaseMultiplier(multiplier, dB);
                        if(newmultiplier != multiplier)
                        {
                            auto newreverb = project.Reverb().ChangeMixLevel(newmultiplier);
                            auto newproject = project.ChangeReverb(std::move(newreverb));
                            m_Engine.SetProject(std::move(newproject));
                        }
                    }
                    else
                    {
                        size_t partindex = (button == Hid::TButtonIndex::LcdRotary6)? 1:0;
                        if(project.Parts().size() > partindex)
                        {
                            auto multiplier = project.Parts().at(partindex).AmplitudeFactor();
                            auto newmultiplier = increaseMultiplier(multiplier, dB);
                            if(newmultiplier != multiplier)
                            {
                                auto newpart = project.Parts().at(partindex).ChangeAmplitudeFactor(newmultiplier);
                                auto newproject = project.ChangePart(partindex, std::move(newpart));
                                m_Engine.SetProject(std::move(newproject));
                            }
                        }
                    }
                }
            }
            if( (button == Hid::TButtonIndex::Clear) && (delta > 0) )
            {
                const auto &project = m_Engine.Project();
                if(project.FocusedPart() && (project.FocusedPart() < project.Parts().size()))
                {
                    m_Engine.SwitchFocusedPartToPreset(m_GuiState.m_SelectedPreset);
                }
            }
            if( (button >= Hid::TButtonIndex::Menu0) && (button <= Hid::TButtonIndex::Menu7) && (delta > 0) )
            {
                size_t buttonindex = (size_t)((int)button - (int)Hid::TButtonIndex::Menu0);
                if( (m_GuiState.m_Mode == TGuiState::TMode::Performance) && m_Engine.Project().FocusedPart() )
                {
                    auto focusedpartindex = m_Engine.Project().FocusedPart().value();
                    const auto &part = project.Parts().at(focusedpartindex);
                    size_t quickPresetIndex = m_GuiState.m_QuickPresetPage * 8 + buttonindex;
                    if(m_GuiState.m_Shift)
                    {
                        // change quick preset:
                        auto newpart = part.ChangeQuickPreset(quickPresetIndex, m_GuiState.m_SelectedPreset);
                        auto newproject = project.ChangePart(focusedpartindex, std::move(newpart));
                        m_Engine.SetProject(std::move(newproject));
                        m_GuiState.m_Shift = false;
                    }
                    else
                    {
                        // activate quick preset:
                        std::optional<size_t> presetIndex;
                        if(quickPresetIndex < part.QuickPresets().size())
                        {
                            presetIndex = part.QuickPresets().at(quickPresetIndex);
                        }
                        if(presetIndex)
                        {
                            m_Engine.SwitchFocusedPartToPreset(*presetIndex);
                        }
                    }
                }
            }
        }
        else if(m_GuiState.m_Mode == TGuiState::TMode::Midi)
        {
            if(button == Hid::TButtonIndex::LcdRotary0)
            {
                auto v = m_GuiState.m_ProgramChangeHysteresis.Update(delta);
                if(v != 0)
                {
                    m_GuiState.m_ProgramChange = (size_t)std::clamp((int)m_GuiState.m_ProgramChange + v, 0, 127);
                    m_Engine.SendProgramChangeOnActivePart(m_GuiState.m_ProgramChange);
                }
            }
        }
        Refresh();
        RefreshLeds();
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
                auto starttime = std::chrono::steady_clock::now();
                // todo: find dirty regions
                PaintWindow(display, *window);
                m_PrevWindow = std::move(window);
                display.SendPixels(0, 0, Display::sWidth, Display::sHeight);
                auto endtime = std::chrono::steady_clock::now();
                [[maybe_unused]] auto elapsed = endtime - starttime;
                // std::cout << "Painting took " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "ms" << std::endl;
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
        if(project.Presets().empty())
        {
            m_GuiState.m_SelectedPreset = 0;
        }
        else
        {
            m_GuiState.m_SelectedPreset = std::min(m_GuiState.m_SelectedPreset, project.Presets().size() - 1);
        }
        Refresh();
        RefreshLeds();
    }
    void Gui::Refresh()
    {
        auto mainwindow = std::make_unique<simplegui::Window>(nullptr, Gdk::Rectangle(0, 0, Display::sWidth, Display::sWidth));
        int fontsize = 15;
        int lineheight = fontsize + 2;
        int linespacing = 2;

        const auto &project = m_Engine.Project();

        if(m_GuiState.m_Mode == TGuiState::TMode::Performance)
        {
            std::string presetname;
            if(m_GuiState.m_SelectedPreset < project.Presets().size())
            {
                const auto &presetOrNull = project.Presets()[m_GuiState.m_SelectedPreset];
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
                auto partcolor = colorForPart(project.FocusedPart().value());
                {
                    int boxwidth = 100;
                    int boxheight = 40;
                    auto box = mainwindow->AddChild<simplegui::PlainWindow>(Gdk::Rectangle(960-boxwidth, (272-boxheight)/2, boxwidth, boxheight), partcolor);
                    box->AddChild<simplegui::TextWindow>(Gdk::Rectangle(3, 3, boxwidth - 6, boxheight - 6), project.Parts().at(project.FocusedPart().value()).Name(), simplegui::Rgba(0, 0, 0), 25);
                }
                int quickPresetsBoxHeight = fontsize + 4;
                int quickPresetHorzPadding = 1;

                auto quickPresetsBar = mainwindow->AddChild<simplegui::Window>(Gdk::Rectangle(0, 0, 960, quickPresetsBoxHeight));
                for(size_t quickPresetOffset = 0; quickPresetOffset < 8; quickPresetOffset++)
                {
                    size_t quickPresetIndex = m_GuiState.m_QuickPresetPage * 8 + quickPresetOffset;
                    std::optional<size_t> presetIndex;
                    if(quickPresetIndex < project.Parts().at(project.FocusedPart().value()).QuickPresets().size())
                    {
                        presetIndex = project.Parts().at(project.FocusedPart().value()).QuickPresets().at(quickPresetIndex);
                    }
                    if(presetIndex)
                    {
                        const project::TPreset *preset = nullptr;
                        if(*presetIndex < project.Presets().size())
                        {
                            const auto &presetOrNull = project.Presets()[*presetIndex];
                            if(presetOrNull)
                            {
                                preset = &*presetOrNull;
                            }
                        }
                        std::string presetName;
                        if(preset)
                        {
                            presetName = preset->Name();
                        }
                        else
                        {
                            presetName = "(empty)";
                        }
                        auto boxcolor = m_GuiState.m_Shift? simplegui::Rgba(0.8, 0.8, 0.8): partcolor;
                        auto quickPresetBox = quickPresetsBar->AddChild<simplegui::PlainWindow>(Gdk::Rectangle(quickPresetOffset * 120 + quickPresetHorzPadding, 0, 120 - 2*quickPresetHorzPadding, quickPresetsBoxHeight), boxcolor);
                        quickPresetBox->AddChild<simplegui::TextWindow>(Gdk::Rectangle(1, 1, quickPresetBox->Width() - 2, quickPresetBox->Height() - 2), presetName, simplegui::Rgba(0, 0, 0), fontsize);
                    }
                }
            }
            bool hasfocusedslider = false;
            simplegui::Rgba focusedslidercolor;
            double focusedslidervalue = 0;
            std::string focusedslidertext;
            int sliderbottom = 272;
            int sliderheight = 30;
            for(int sliderindex = 0; sliderindex < 3; sliderindex++)
            {
                int sliderleft = (5+sliderindex)*120 + 5;
                int sliderright = sliderleft + 120 - 10;
                double multiplier;
                simplegui::Rgba slidercolor = simplegui::Rgba(0.7, 0.7, 0.7);
                if(sliderindex == 0)
                {
                    multiplier = project.Reverb().MixLevel();
                }
                else
                {
                    size_t partindex = (size_t)(2-sliderindex);
                    multiplier = 0;
                    if(project.Parts().size() > partindex)
                    {
                        multiplier = project.Parts().at(partindex).AmplitudeFactor();
                    }
                    slidercolor = colorForPart(partindex);
                }
                double slidervalue = multiplierToSliderValue(multiplier);
                auto slidertext = multiplierToText(multiplier);

                auto slider = mainwindow->AddChild<simplegui::TSlider>(Gdk::Rectangle(sliderleft, sliderbottom - sliderheight, sliderright - sliderleft, sliderheight), slidertext, slidervalue, slidercolor);
                bool isfocused = m_Hid.GetButtonState(Hid::TButtonIndex(int(Hid::TButtonIndex::TouchLcdRotary5) + sliderindex)) != 0;
                if(isfocused && (!hasfocusedslider))
                {
                    hasfocusedslider = true;
                    focusedslidercolor = slidercolor;
                    focusedslidervalue = slidervalue;
                    focusedslidertext = slidertext;
                }
            }
            if(hasfocusedslider)
            {
                int focusedsliderleft = 480 + 40;
                int focusedsliderright = 960 - 40;
                int focusedsliderbottom = sliderbottom - sliderheight;
                int focusedsliderheight = 80;
                auto focusedslider = mainwindow->AddChild<simplegui::TSlider>(Gdk::Rectangle(focusedsliderleft, focusedsliderbottom - focusedsliderheight, focusedsliderright - focusedsliderleft, focusedsliderheight), focusedslidertext, focusedslidervalue, focusedslidercolor);
            }

            if(m_Hid.GetButtonState(Hid::TButtonIndex::TouchLcdRotary0) != 0)
            {
                // show popup selector:
                mainwindow->AddChild<simplegui::TListBox>(Gdk::Rectangle(10, 272/2 - 100, 400, 272/2+100), simplegui::Rgba(1,1,1), 25, project.Presets().size(), m_GuiState.m_SelectedPreset, m_GuiState.m_SelectedPreset, [this, &project](size_t index) -> std::string {
                    auto result = std::to_string(index) + ": ";
                    if(index < project.Presets().size())
                    {
                        const auto &presetOrNull = project.Presets()[index];
                        if(presetOrNull)
                        {
                            result += presetOrNull->Name();
                        }
                    }
                    return result;
                });
            }

        } // if performance
        else if(m_GuiState.m_Mode == TGuiState::TMode::Midi)
        {
            int presetboxheight = 2*lineheight + linespacing + 2;
            auto presetnamebox = mainwindow->AddChild<simplegui::PlainWindow>(Gdk::Rectangle(0, 272-presetboxheight, 120, presetboxheight), simplegui::Rgba(0.2, 0.2, 0.2));

            presetnamebox->AddChild<simplegui::TextWindow>(Gdk::Rectangle(1, 1, presetnamebox->Width() - 2, lineheight), "Program", simplegui::Rgba(1, 1, 1), fontsize);

            presetnamebox->AddChild<simplegui::TextWindow>(Gdk::Rectangle(1, 1 + lineheight + linespacing, presetnamebox->Width() - 2, lineheight), std::to_string(m_GuiState.m_ProgramChange), simplegui::Rgba(1, 1, 1), fontsize);            
        }

        SetWindow(std::move(mainwindow));
    }
    void Gui::RefreshLeds()
    {
        const auto &project = m_Engine.Project();
        m_Hid.SetLed(Hid::TButtonIndex::Shift, m_GuiState.m_Shift? 4:1);
        m_Hid.SetLed(Hid::TButtonIndex::Instance, 2);
        m_Hid.SetLed(Hid::TButtonIndex::Midi, m_GuiState.m_Mode == TGuiState::TMode::Midi? 4:2);
        m_Hid.SetLed(Hid::TButtonIndex::Browser, m_GuiState.m_Mode == TGuiState::TMode::Performance? 4:2);
        m_Hid.SetLed(Hid::TButtonIndex::Arp, project.ShowUi()? 4:2);

        if(m_GuiState.m_Mode == TGuiState::TMode::Midi)
        {
            m_Hid.SetLed(Hid::TButtonIndex::Clear, 0);
            for(size_t btnIndex = 0; btnIndex < 8; btnIndex++)
            {
                m_Hid.SetLed(Hid::TButtonIndex(int(Hid::TButtonIndex::Menu0) + (int)btnIndex), 0);
            }
        }
        else if(m_GuiState.m_Mode == TGuiState::TMode::Performance)
        {
            m_Hid.SetLed(Hid::TButtonIndex::Clear, 2);
            for(size_t quickPresetOffset = 0; quickPresetOffset < 8; quickPresetOffset++)
            {
                size_t quickPresetIndex = m_GuiState.m_QuickPresetPage * 8 + quickPresetOffset;
                auto button = Hid::TButtonIndex(int(Hid::TButtonIndex::Menu0) + (int)quickPresetOffset);
                if(project.FocusedPart())
                {
                    const auto &part = project.Parts().at(project.FocusedPart().value());
                    auto ledcolor = (*project.FocusedPart() == 0)? Hid::TLedColor::Red : Hid::TLedColor::Green;
                    if(m_GuiState.m_Shift)
                    {
                        m_Hid.SetLed(button, ledcolor, 3);
                    }
                    else
                    {
                        std::optional<size_t> presetIndex;
                        if(quickPresetIndex< part.QuickPresets().size())
                        {
                            presetIndex = part.QuickPresets().at(quickPresetIndex);
                        }
                        bool isSelected = presetIndex && (part.ActivePresetIndex() == presetIndex);
                        if(isSelected)
                        {
                            m_Hid.SetLed(button, ledcolor, 3);
                        }
                        else
                        {
                            m_Hid.SetLed(button, 1);
                        }
                    }
                }
                else
                {
                    m_Hid.SetLed(button, 0);
                }
            }
        }
    }
}
