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

    class TDrawbarKnob : public simplegui::Window
    {
    public:
        TDrawbarKnob(Window *parent, const Gdk::Rectangle &rect, const simplegui::Rgba &color) : m_Color(color), Window(parent, rect)
        {        
        }
        void DoPaint(Cairo::Context &cr) const override
        {
            // draw a rounded rectangle:
            cr.set_source_rgba(m_Color.Red(), m_Color.Green(), m_Color.Blue(), m_Color.Alpha());
            auto rect = Rectangle();
            double linewidth = 1.0;
            int top1 = 0;
            int top2 = 0 + 2 * rect.get_height() / 3;
            int bottom = 0 + rect.get_height();
            double cornerradius = (bottom - top2 - linewidth) / 2.01;
            cr.begin_new_path();
            cr.move_to(linewidth/2, top1 + linewidth/2);
            cr.arc_negative(linewidth/2 + cornerradius, bottom - cornerradius - linewidth/2, cornerradius, 180.0 * M_PI / 180.0, 90.0 * M_PI / 180.0);
            cr.arc_negative(rect.get_width() - cornerradius - linewidth/2, bottom - cornerradius - linewidth/2, cornerradius, 90.0 * M_PI / 180.0, 0.0 * M_PI / 180.0);
            cr.line_to(rect.get_width() - linewidth/2, top1);
            cr.close_path();
            cr.fill_preserve();
            cr.set_source_rgba(1.0, 1.0, 1.0, 1.0);
            cr.set_line_width(linewidth);
            cr.stroke();
            cr.move_to(linewidth/2, top2 + cornerradius + linewidth/2);
            cr.arc_negative(linewidth/2 + cornerradius, bottom - cornerradius - linewidth/2, cornerradius, 180.0 * M_PI / 180.0, 90.0 * M_PI / 180.0);
            cr.arc_negative(rect.get_width() - cornerradius - linewidth/2, bottom - cornerradius - linewidth/2, cornerradius, 90.0 * M_PI / 180.0, 0.0 * M_PI / 180.0);
            cr.arc_negative(rect.get_width() - cornerradius - linewidth/2, top2 + cornerradius + linewidth/2, cornerradius, 0.0 * M_PI / 180.0, 270.0 * M_PI / 180.0);
            cr.arc_negative(linewidth/2 + cornerradius, top2 + cornerradius + linewidth/2, cornerradius, 270.0 * M_PI / 180.0, 180.0 * M_PI / 180.0);
            cr.close_path();
            cr.stroke();
        }
    private:
        simplegui::Rgba m_Color;
    };

    class TDrawbarWindow : public simplegui::Window
    {
    public:
        TDrawbarWindow(Window *parent, const Gdk::Rectangle &rect, const simplegui::Rgba &color, int position) : Window(parent, rect)
        {
            position = std::clamp(position, 0, 8);
            int buttonheight = rect.get_height() / 3;
            int buttontop_full = rect.get_height() - buttonheight;
            int buttontop = 0 + position * buttontop_full / 8;
            int fontsize = buttontop_full / 10;
            int whitewidth = 9 * rect.get_width() / 10;
            int blackwidth = rect.get_width() / 3;
            auto whitebox = AddChild<simplegui::PlainWindow>(Gdk::Rectangle(rect.get_width() / 2 - whitewidth / 2, 0, whitewidth, buttontop), simplegui::Rgba(1, 1, 1, 1));
            auto blackbox = whitebox->AddChild<simplegui::PlainWindow>(Gdk::Rectangle(whitebox->Rectangle().get_width() / 2 - blackwidth / 2, 0, blackwidth, whitebox->Rectangle().get_height()), simplegui::Rgba(0, 0, 0, 1));
            for(int p = 1; p <= position; p++)
            {
                int boxtop = (position - p) * buttontop_full / 8;
                int boxbottom = (position - p + 1) * buttontop_full / 8;
                blackbox->AddChild<simplegui::TextWindow>(Gdk::Rectangle(0, boxtop, blackwidth, boxbottom - boxtop), std::to_string(p), simplegui::Rgba(1, 1, 1, 1), fontsize, simplegui::TextWindow::THalign::Center);
            }
            AddChild<TDrawbarKnob>(Gdk::Rectangle(0, buttontop, rect.get_width(), buttonheight), color);
        }

    private:
        int m_ButtonHeight;

    };

    std::optional<size_t> TGuiState::ActivePartIsHammond() const
    {
        const auto &project = EngineData().Project();
        if(m_FocusedPart &&  project.Parts().at(m_FocusedPart.value()).ActiveInstrumentIndex())
        {
            const auto &instrument = project.Instruments().at(project.Parts().at(m_FocusedPart.value()).ActiveInstrumentIndex().value());
            if(instrument.IsHammond())
            {
                auto hammondchannel = project.Parts().at(m_FocusedPart.value()).MidiChannelForSharedInstruments();
                if( (hammondchannel >= 0) && (hammondchannel <= 1))
                {
                    return (size_t)hammondchannel;
                }
            }
        }
        return std::nullopt;
    }
    void TGuiState::SetEngineData(const engine::Engine::TData &engineData)
    {
        m_EngineData = engineData;
        if(m_EngineData.Project().Presets().empty())
        {
            m_SelectedPreset = 0;
        }
        else
        {
            m_SelectedPreset = std::min(m_SelectedPreset, m_EngineData.Project().Presets().size() - 1);
        }
        if(m_FocusedPart)
        {
            if(m_EngineData.Project().Parts().empty())
            {
                m_FocusedPart = std::nullopt;
            }
            else
            {
                m_FocusedPart = std::min(*m_FocusedPart, m_EngineData.Project().Parts().size() - 1);
            }
        }
        else
        {
            if(!m_EngineData.Project().Parts().empty())
            {
                m_FocusedPart = 0;
            }
        }
    }

    void Gui::SetGuiState(TGuiState &&state)
    {
        m_GuiState1 = std::move(state);
        RefreshLcd();
        RefreshLeds();
    }

    Gui::Gui(std::pair<int, int> vidPid, engine::Engine &engine) : m_Engine(engine), m_Hid(vidPid, [this](Hid::TButtonIndex button, int delta) { OnButton(button, delta); })
    {
        m_GuiThread = std::thread([vidPid, this]() {
            RunGuiThread(vidPid);
        });
        auto guistate = GuiState();
        guistate.SetEngineData(m_Engine.Data());
        SetGuiState(std::move(guistate));
    }

    void Gui::OnButton(Hid::TButtonIndex button, int delta)
    {
        if( (button == Hid::TButtonIndex::Shift) && (delta > 0) )
        {
            auto guistate = GuiState();
            guistate.m_Shift = !guistate.m_Shift;
            SetGuiState(std::move(guistate));
        }
        if( (button == Hid::TButtonIndex::Browser) && (delta > 0) )
        {
            auto guistate = GuiState();
            guistate.m_Mode = TGuiState::TMode::Performance;
            SetGuiState(std::move(guistate));
        }
        if( (button == Hid::TButtonIndex::Midi) && (delta > 0) )
        {
            auto guistate = GuiState();
            guistate.m_Mode = TGuiState::TMode::Midi;
            SetGuiState(std::move(guistate));
        }
        if( (button == Hid::TButtonIndex::Plugin) && (delta > 0) )
        {
            auto guistate = GuiState();
            guistate.m_Mode = TGuiState::TMode::Controller;
            SetGuiState(std::move(guistate));
        }
        if( (button == Hid::TButtonIndex::Arp) && (delta > 0) )
        {
            if(GuiState().m_FocusedPart)
            {
                auto newenginedata = GuiState().EngineData().ChangeShowUi(!GuiState().EngineData().ShowUi());
                if(newenginedata.ShowUi())
                {
                    newenginedata = newenginedata.ChangeGuiFocusedPart(GuiState().m_FocusedPart);
                }
                m_Engine.SetData(std::move(newenginedata));
            }
        }
        if( (button == Hid::TButtonIndex::Instance) && (delta > 0) )
        {
            auto numparts = GuiState().EngineData().Project().Parts().size();
            std::optional<size_t> newfocusedpart;
            if(numparts > 0)
            {
                auto newguistate = GuiState();
                if(newguistate.m_FocusedPart)
                {
                    newguistate.m_FocusedPart = (*newguistate.m_FocusedPart) + 1;
                    if(newguistate.m_FocusedPart >= numparts)
                    {
                        newguistate.m_FocusedPart = 0;
                    }
                }
                else
                {
                    newguistate.m_FocusedPart = 0;
                }
                SetGuiState(std::move(newguistate));
            }
        }
        if(GuiState().m_Mode == TGuiState::TMode::Controller)
        {
            auto partOrNull = GuiState().ActivePartIsHammond();
            if(partOrNull)
            {
                if( ((button >= Hid::TButtonIndex::LcdRotary0) && (button <= Hid::TButtonIndex::LcdRotary7)) || (button == Hid::TButtonIndex::BigRotary))
                {
                    size_t drawbarindex = (button == Hid::TButtonIndex::BigRotary)? 8 : ((size_t)((int)button - (int)Hid::TButtonIndex::LcdRotary0));
                    int v = m_DrawbarHysteresis[drawbarindex].Update(delta);
                    if(v != 0)
                    {
                        const auto &hammonddata = GuiState().EngineData().HammondData();
                        const auto &hammondpart = hammonddata.Part(*partOrNull);
                        auto newvalue = std::clamp(hammondpart.Registers()[drawbarindex] + v, 0, 8);
                        if(newvalue != hammondpart.Registers()[drawbarindex])
                        {
                            auto newhammondpart = hammondpart.ChangeRegister(drawbarindex, newvalue);
                            auto newhammonddata = hammonddata.ChangePart(*partOrNull, std::move(newhammondpart));
                            auto newenginedata = GuiState().EngineData().ChangeHammondData(std::move(newhammonddata));
                            m_Engine.SetData(std::move(newenginedata));
                        }
                    }
                }
                if( (button >= Hid::TButtonIndex::Menu0) && (button <= Hid::TButtonIndex::Menu7) && (delta > 0) )
                {
                    auto hammonddata = GuiState().EngineData().HammondData();
                    if(*partOrNull == 0)
                    {
                        if(button == Hid::TButtonIndex::Menu0)
                        {
                            hammonddata = hammonddata.ChangePercussion(!hammonddata.Percussion());
                        }
                        if(button == Hid::TButtonIndex::Menu1)
                        {
                            hammonddata = hammonddata.ChangePercussionSoft(!hammonddata.PercussionSoft());
                        }
                        if(button == Hid::TButtonIndex::Menu2)
                        {
                            hammonddata = hammonddata.ChangePercussionFast(!hammonddata.PercussionFast());
                        }
                        if(button == Hid::TButtonIndex::Menu3)
                        {
                            hammonddata = hammonddata.ChangePercussion2ndHarmonic(!hammonddata.Percussion2ndHarmonic());
                        }
                    }
                    auto newenginedata = GuiState().EngineData().ChangeHammondData(std::move(hammonddata));
                    m_Engine.SetData(std::move(newenginedata));                    
                }
            }
        }

        if(GuiState().m_Mode == TGuiState::TMode::Performance)
        {
            if(button == Hid::TButtonIndex::LcdRotary0)
            {
                auto v = m_SelectedPresetHysteresis.Update(delta);
                auto numpresets = GuiState().EngineData().Project().Presets().size();
                if(numpresets > 0)
                {
                    if(v != 0)
                    {
                        auto newguistate = GuiState();
                        newguistate.m_SelectedPreset = (size_t)std::clamp((int)newguistate.m_SelectedPreset + v, 0, (int)numpresets - 1);
                        SetGuiState(std::move(newguistate));
                    }
                }
            }
            if( (button >= Hid::TButtonIndex::LcdRotary5) && (button <= Hid::TButtonIndex::LcdRotary7) )
            {
                utils::THysteresis &hysteresis = (button == Hid::TButtonIndex::LcdRotary5)? m_ReverbLevelHysteresis : (button == Hid::TButtonIndex::LcdRotary6)? m_Part1LevelHysteresis : m_Part2LevelHysteresis;
                auto v = hysteresis.Update(delta);
                if(v != 0)
                {
                    double dB = v * 0.5;
                    if(button == Hid::TButtonIndex::LcdRotary5)
                    {
                        auto multiplier = GuiState().EngineData().Project().Reverb().MixLevel();
                        auto newmultiplier = increaseMultiplier(multiplier, dB);
                        if(newmultiplier != multiplier)
                        {
                            auto newreverb = GuiState().EngineData().Project().Reverb().ChangeMixLevel(newmultiplier);
                            auto newproject = GuiState().EngineData().Project().ChangeReverb(std::move(newreverb));
                            auto newenginedata = GuiState().EngineData().ChangeProject(std::move(newproject));
                            m_Engine.SetData(std::move(newenginedata));
                        }
                    }
                    else
                    {
                        size_t partindex = (button == Hid::TButtonIndex::LcdRotary6)? 1:0;
                        if(GuiState().EngineData().Project().Parts().size() > partindex)
                        {
                            auto multiplier = GuiState().EngineData().Project().Parts().at(partindex).AmplitudeFactor();
                            auto newmultiplier = increaseMultiplier(multiplier, dB);
                            if(newmultiplier != multiplier)
                            {
                                auto newpart = GuiState().EngineData().Project().Parts().at(partindex).ChangeAmplitudeFactor(newmultiplier);
                                auto newproject = GuiState().EngineData().Project().ChangePart(partindex, std::move(newpart));
                                auto newenginedata = GuiState().EngineData().ChangeProject(std::move(newproject));
                                m_Engine.SetData(std::move(newenginedata));
                            }
                        }
                    }
                }
            }
            if( (button == Hid::TButtonIndex::Clear) && (delta > 0) )
            {
                const auto &project = GuiState().EngineData().Project();
                if(GuiState().m_FocusedPart)
                {
                    auto newproject = m_Engine.Project().SwitchToPreset(GuiState().m_FocusedPart.value(), GuiState().m_SelectedPreset);
                    m_Engine.SetProject(std::move(newproject));
                }
            }
            if( (button >= Hid::TButtonIndex::Menu0) && (button <= Hid::TButtonIndex::Menu7) && (delta > 0) )
            {
                size_t buttonindex = (size_t)((int)button - (int)Hid::TButtonIndex::Menu0);
                if(GuiState().m_FocusedPart)
                {
                    auto focusedpartindex = GuiState().m_FocusedPart.value();
                    const auto &part = GuiState().EngineData().Project().Parts().at(focusedpartindex);
                    size_t quickPresetIndex = GuiState().m_QuickPresetPage * 8 + buttonindex;
                    if(GuiState().m_Shift)
                    {
                        // change quick preset:
                        auto newpart = part.ChangeQuickPreset(quickPresetIndex, GuiState().m_SelectedPreset);
                        auto newproject = GuiState().EngineData().Project().ChangePart(focusedpartindex, std::move(newpart));
                        m_Engine.SetProject(std::move(newproject));
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
                            auto newproject = GuiState().EngineData().Project().SwitchToPreset(focusedpartindex, *presetIndex);
                            m_Engine.SetProject(std::move(newproject));
                        }
                    }
                }
            }
        }
        else if(GuiState().m_Mode == TGuiState::TMode::Midi)
        {
            if(button == Hid::TButtonIndex::LcdRotary0)
            {
                auto v = m_ProgramChangeHysteresis.Update(delta);
                if(v != 0)
                {
                    if(GuiState().m_FocusedPart )
                    {
                        auto focusedpartindex = GuiState().m_FocusedPart.value();
                        auto guistate = GuiState();
                        guistate.m_ProgramChange = (size_t)std::clamp((int)guistate.m_ProgramChange + v, 0, 127);
                        SetGuiState(std::move(guistate));
                        m_Engine.SendMidiToPart(midi::SimpleEvent::ProgramChange(0, guistate.m_ProgramChange), focusedpartindex);
                    }
                }
            }
        }
        RefreshLcd(); // touched buttons may affect the gui
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
    void Gui::OnDataChanged()
    {
        auto newstate = GuiState();
        newstate.SetEngineData(m_Engine.Data());
        SetGuiState(std::move(newstate));
    }
    void Gui::PaintPerformanceWindow(simplegui::Window &window) const
    {
        const auto &project = GuiState().EngineData().Project();
        std::string presetname;
        if(GuiState().m_SelectedPreset < project.Presets().size())
        {
            const auto &presetOrNull = project.Presets()[GuiState().m_SelectedPreset];
            if(presetOrNull)
            {
                presetname = presetOrNull->Name();
            }
        }
        int lineheight = sFontSize + 2;
        int presetboxheight = 2*lineheight + sLineSpacing + 2;
        auto presetnamebox = window.AddChild<simplegui::PlainWindow>(Gdk::Rectangle(0, 272-presetboxheight, 120, presetboxheight), simplegui::Rgba(0.2, 0.2, 0.2));

        presetnamebox->AddChild<simplegui::TextWindow>(Gdk::Rectangle(1, 1, presetnamebox->Width() - 2, lineheight), std::to_string(GuiState().m_SelectedPreset), simplegui::Rgba(1, 1, 1), sFontSize, simplegui::TextWindow::THalign::Left);

        presetnamebox->AddChild<simplegui::TextWindow>(Gdk::Rectangle(1, 1 + lineheight + sLineSpacing, presetnamebox->Width() - 2, lineheight), presetname, simplegui::Rgba(1, 1, 1), sFontSize, simplegui::TextWindow::THalign::Left);

        if(GuiState().m_FocusedPart)
        {
            auto partcolor = colorForPart(GuiState().m_FocusedPart.value());
            {
                int boxwidth = 100;
                int boxheight = 40;
                auto box = window.AddChild<simplegui::PlainWindow>(Gdk::Rectangle(960-boxwidth, (272-boxheight)/2, boxwidth, boxheight), partcolor);
                box->AddChild<simplegui::TextWindow>(Gdk::Rectangle(3, 3, boxwidth - 6, boxheight - 6), project.Parts().at(GuiState().m_FocusedPart.value()).Name(), simplegui::Rgba(0, 0, 0), 25, simplegui::TextWindow::THalign::Left);
            }
            int quickPresetsBoxHeight = sFontSize + 4;
            int quickPresetHorzPadding = 1;

            auto quickPresetsBar = window.AddChild<simplegui::Window>(Gdk::Rectangle(0, 0, 960, quickPresetsBoxHeight));
            for(size_t quickPresetOffset = 0; quickPresetOffset < 8; quickPresetOffset++)
            {
                size_t quickPresetIndex = GuiState().m_QuickPresetPage * 8 + quickPresetOffset;
                std::optional<size_t> presetIndex;
                if(quickPresetIndex < project.Parts().at(GuiState().m_FocusedPart.value()).QuickPresets().size())
                {
                    presetIndex = project.Parts().at(GuiState().m_FocusedPart.value()).QuickPresets().at(quickPresetIndex);
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
                    auto boxcolor = GuiState().m_Shift? simplegui::Rgba(0.8, 0.8, 0.8): partcolor;
                    auto quickPresetBox = quickPresetsBar->AddChild<simplegui::PlainWindow>(Gdk::Rectangle(quickPresetOffset * 120 + quickPresetHorzPadding, 0, 120 - 2*quickPresetHorzPadding, quickPresetsBoxHeight), boxcolor);
                    quickPresetBox->AddChild<simplegui::TextWindow>(Gdk::Rectangle(1, 1, quickPresetBox->Width() - 2, quickPresetBox->Height() - 2), presetName, simplegui::Rgba(0, 0, 0), sFontSize, simplegui::TextWindow::THalign::Left);
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

            auto slider = window.AddChild<simplegui::TSlider>(Gdk::Rectangle(sliderleft, sliderbottom - sliderheight, sliderright - sliderleft, sliderheight), slidertext, slidervalue, slidercolor);
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
            auto focusedslider = window.AddChild<simplegui::TSlider>(Gdk::Rectangle(focusedsliderleft, focusedsliderbottom - focusedsliderheight, focusedsliderright - focusedsliderleft, focusedsliderheight), focusedslidertext, focusedslidervalue, focusedslidercolor);
        }

        if(m_Hid.GetButtonState(Hid::TButtonIndex::TouchLcdRotary0) != 0)
        {
            // show popup selector:
            window.AddChild<simplegui::TListBox>(Gdk::Rectangle(10, 272/2 - 100, 400, 272/2+100), simplegui::Rgba(1,1,1), 25, project.Presets().size(), GuiState().m_SelectedPreset, GuiState().m_SelectedPreset, [this, &project](size_t index) -> std::string {
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
    }
    void Gui::PaintMidiWindow(simplegui::Window &window) const
    {
        int lineheight = sFontSize + 2;
        const auto &project = GuiState().EngineData().Project();
        int presetboxheight = 2*lineheight + sLineSpacing + 2;
        auto presetnamebox = window.AddChild<simplegui::PlainWindow>(Gdk::Rectangle(0, 272-presetboxheight, 120, presetboxheight), simplegui::Rgba(0.2, 0.2, 0.2));

        presetnamebox->AddChild<simplegui::TextWindow>(Gdk::Rectangle(1, 1, presetnamebox->Width() - 2, lineheight), "Program", simplegui::Rgba(1, 1, 1), sFontSize, simplegui::TextWindow::THalign::Left);

        presetnamebox->AddChild<simplegui::TextWindow>(Gdk::Rectangle(1, 1 + lineheight + sLineSpacing, presetnamebox->Width() - 2, lineheight), std::to_string(GuiState().m_ProgramChange), simplegui::Rgba(1, 1, 1), sFontSize, simplegui::TextWindow::THalign::Left);            
    }
    void Gui::PaintHammondControllerWindow(simplegui::Window &window, size_t part) const
    {
        int lineheight = sFontSize + 2;
        const auto &hammonddata = GuiState().EngineData().HammondData();
        const auto &hammondpart = hammonddata.Part(part);
        int drawbartop = 50;
        int drawbarbottom = 250;
        simplegui::Rgba brown(143/255.0, 69/255.0, 0.0, 1.0);
        for(size_t drawbarindex = 0; drawbarindex < 9; drawbarindex++)
        {
            int drawbarwidth = 80;
            int drawbarwidth_padded = drawbarwidth + 10;
            int drawbarcenter = 60 + (int)drawbarindex * 120;
            if(drawbarindex >= 4)
            {
                int drawbarcenter2 = 960 - drawbarwidth/2 - (8-drawbarindex) * drawbarwidth_padded;
                drawbarcenter = std::min(drawbarcenter, drawbarcenter2);
            }
            int value = hammondpart.Registers()[drawbarindex];
            value = std::clamp(value, 0, 8);
            window.AddChild<TDrawbarWindow>(Gdk::Rectangle(drawbarcenter - drawbarwidth/2, drawbartop, drawbarwidth, drawbarbottom - drawbartop), brown, value);
        }
        int menuboxheight = sFontSize + 4;
        int menuhorzpadding = 1;
        auto partcolor = colorForPart(part);
        simplegui::Rgba black(0, 0, 0);
        if(part == 0)
        {
            for(size_t buttonindex = 0; buttonindex < 4; buttonindex++)
            {
                bool value = buttonindex == 0? hammonddata.Percussion() : (buttonindex == 1? hammonddata.PercussionSoft() : (buttonindex == 2? hammonddata.PercussionFast(): hammonddata.Percussion2ndHarmonic()));
                std::string label = buttonindex == 0? "Percussion" : (buttonindex == 1? "Soft" : (buttonindex == 2? "Fast": "2nd"));
                auto menubox = window.AddChild<simplegui::PlainWindow>(Gdk::Rectangle(buttonindex * 120 + menuhorzpadding, 0, 120 - 2*menuhorzpadding, menuboxheight), value? partcolor:black);
                menubox->AddChild<simplegui::TextWindow>(Gdk::Rectangle(1, 1, menubox->Width() - 2, menubox->Height() - 2), label, value? black:partcolor, sFontSize, simplegui::TextWindow::THalign::Center);
            }
        }
    }
    void Gui::PaintControllerWindow(simplegui::Window &window) const
    {
        auto partOrNull = GuiState().ActivePartIsHammond();
        if(partOrNull)
        {
            PaintHammondControllerWindow(window, *partOrNull);            
        }
    }
    void Gui::RefreshLcd()
    {
        auto mainwindow = std::make_unique<simplegui::Window>(nullptr, Gdk::Rectangle(0, 0, Display::sWidth, Display::sWidth));
        int lineheight = sFontSize + 2;

        const auto &project = GuiState().EngineData().Project();

        if(GuiState().m_Mode == TGuiState::TMode::Performance)
        {
            PaintPerformanceWindow(*mainwindow);
        }
        else if(GuiState().m_Mode == TGuiState::TMode::Midi)
        {
            PaintMidiWindow(*mainwindow);
        }
        if(GuiState().m_Mode == TGuiState::TMode::Controller)
        {
            PaintControllerWindow(*mainwindow);
        }
        SetWindow(std::move(mainwindow));
    }
    void Gui::RefreshLeds()
    {
        const auto &project = GuiState().EngineData().Project();
        m_Hid.SetLed(Hid::TButtonIndex::Shift, GuiState().m_Shift? 4:1);
        m_Hid.SetLed(Hid::TButtonIndex::Instance, 2);
        m_Hid.SetLed(Hid::TButtonIndex::Midi, GuiState().m_Mode == TGuiState::TMode::Midi? 4:2);
        m_Hid.SetLed(Hid::TButtonIndex::Browser, GuiState().m_Mode == TGuiState::TMode::Performance? 4:2);
        m_Hid.SetLed(Hid::TButtonIndex::Arp, GuiState().EngineData().ShowUi()? 4:2);
        m_Hid.SetLed(Hid::TButtonIndex::Plugin, GuiState().m_Mode == TGuiState::TMode::Controller? 4:2);

        if(GuiState().m_Mode == TGuiState::TMode::Midi)
        {
            m_Hid.SetLed(Hid::TButtonIndex::Clear, 0);
            for(size_t btnIndex = 0; btnIndex < 8; btnIndex++)
            {
                m_Hid.SetLed(Hid::TButtonIndex(int(Hid::TButtonIndex::Menu0) + (int)btnIndex), 0);
            }
        }
        else if(GuiState().m_Mode == TGuiState::TMode::Performance)
        {
            m_Hid.SetLed(Hid::TButtonIndex::Clear, 2);
            for(size_t quickPresetOffset = 0; quickPresetOffset < 8; quickPresetOffset++)
            {
                size_t quickPresetIndex = GuiState().m_QuickPresetPage * 8 + quickPresetOffset;
                auto button = Hid::TButtonIndex(int(Hid::TButtonIndex::Menu0) + (int)quickPresetOffset);
                if(GuiState().m_FocusedPart)
                {
                    const auto &part = project.Parts().at(GuiState().m_FocusedPart.value());
                    auto ledcolor = (*GuiState().m_FocusedPart == 0)? Hid::TLedColor::Red : Hid::TLedColor::Green;
                    if(GuiState().m_Shift)
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
        else if(GuiState().m_Mode == TGuiState::TMode::Controller)
        {
            auto ledcolor = (*GuiState().m_FocusedPart == 0)? Hid::TLedColor::Red : Hid::TLedColor::Green;
            auto partOrNull = GuiState().ActivePartIsHammond();
            if(partOrNull)
            {
                const auto &hammonddata = GuiState().EngineData().HammondData();
                for(size_t btnIndex = 0; btnIndex < 8; btnIndex++)
                {
                    bool isSelected = false;
                    if(*partOrNull == 0)
                    {
                        if(btnIndex == 0)
                        {
                            isSelected = hammonddata.Percussion();
                        }
                        else if(btnIndex == 1)
                        {
                            isSelected = hammonddata.PercussionSoft();
                        }
                        else if(btnIndex == 2)
                        {
                            isSelected = hammonddata.PercussionFast();
                        }
                        else if(btnIndex == 3)
                        {
                            isSelected = hammonddata.Percussion2ndHarmonic();
                        }
                    }
                    if(isSelected)
                    {
                        m_Hid.SetLed(Hid::TButtonIndex(int(Hid::TButtonIndex::Menu0) + (int)btnIndex), ledcolor, 3);
                    }
                    else
                    {
                        m_Hid.SetLed(Hid::TButtonIndex(int(Hid::TButtonIndex::Menu0) + (int)btnIndex), ledcolor, 1);
                    }
                }
            }
            else
            {
                for(size_t btnIndex = 0; btnIndex < 8; btnIndex++)
                {
                    m_Hid.SetLed(Hid::TButtonIndex(int(Hid::TButtonIndex::Menu0) + (int)btnIndex), 0);
                }
            }
        }
    }
}
