#include "komplete.h"
#include <thread>
#include "kompletegui.h"
#include "simplegui.h"

using std::chrono_literals::operator""ms;

namespace komplete
{
    constexpr int sNumPresetPages = 16;
    constexpr double sMinSliderValue = -60;
    constexpr double sMaxSliderValue = 10;
    constexpr double sSliderPow = 0.6;

    double dbToSliderValue(double db)
    {
        auto slidervalue = (db - sMinSliderValue) / (sMaxSliderValue - sMinSliderValue);
        slidervalue = std::clamp(slidervalue, 0.0, 1.0);
        slidervalue = std::pow(slidervalue, sSliderPow);
        return slidervalue;
    }

    double multiplierToDb(double multiplier)
    {
        if(multiplier <= 0.0) return - std::numeric_limits<double>::infinity();
        return 20 * log10(multiplier);
    }

    double multiplierToSliderValue(double multiplier)
    {
        return dbToSliderValue(multiplierToDb(multiplier));
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

    utils::TFloatColor colorForPart(size_t partindex)
    {
        return (partindex == 0)? utils::TFloatColor(1, 0.3, 0.3): utils::TFloatColor(0.3, 1, 0.3);
    }

    std::string dbToText(double db)
    {
        if(db <= sMinSliderValue) return "-inf";
        int dbRounded = (int)std::lround(db);
        if(dbRounded > 0)
        {
            return "+" + std::to_string(dbRounded);
        }
        return std::to_string(dbRounded);
    }

    std::string multiplierToText(double v)
    {
        return dbToText(multiplierToDb(v));
    }

    class TDrawbarKnob : public simplegui::Window
    {
    public:
        TDrawbarKnob(Window *parent, const utils::TIntRect &rect, const utils::TFloatColor &color) : m_Color(color), Window(parent, rect)
        {        
        }
        void DoPaint(Cairo::Context &cr) const override
        {
            simplegui::Window::DoPaint(cr);
            // draw a rounded rectangle:
            cr.set_source_rgba(m_Color.Red(), m_Color.Green(), m_Color.Blue(), m_Color.Alpha());
            auto rect = Rectangle();
            double linewidth = 1.0;
            int top1 = 0;
            int top2 = 0 + 2 * rect.Height() / 3;
            int bottom = 0 + rect.Height();
            double cornerradius = (bottom - top2 - linewidth) / 2.01;
            cr.begin_new_path();
            cr.move_to(linewidth/2, top1 + linewidth/2);
            cr.arc_negative(linewidth/2 + cornerradius, bottom - cornerradius - linewidth/2, cornerradius, 180.0 * M_PI / 180.0, 90.0 * M_PI / 180.0);
            cr.arc_negative(rect.Width() - cornerradius - linewidth/2, bottom - cornerradius - linewidth/2, cornerradius, 90.0 * M_PI / 180.0, 0.0 * M_PI / 180.0);
            cr.line_to(rect.Width() - linewidth/2, top1);
            cr.close_path();
            cr.fill_preserve();
            cr.set_source_rgba(1.0, 1.0, 1.0, 1.0);
            cr.set_line_width(linewidth);
            cr.stroke();
            cr.move_to(linewidth/2, top2 + cornerradius + linewidth/2);
            cr.arc_negative(linewidth/2 + cornerradius, bottom - cornerradius - linewidth/2, cornerradius, 180.0 * M_PI / 180.0, 90.0 * M_PI / 180.0);
            cr.arc_negative(rect.Width() - cornerradius - linewidth/2, bottom - cornerradius - linewidth/2, cornerradius, 90.0 * M_PI / 180.0, 0.0 * M_PI / 180.0);
            cr.arc_negative(rect.Width() - cornerradius - linewidth/2, top2 + cornerradius + linewidth/2, cornerradius, 0.0 * M_PI / 180.0, 270.0 * M_PI / 180.0);
            cr.arc_negative(linewidth/2 + cornerradius, top2 + cornerradius + linewidth/2, cornerradius, 270.0 * M_PI / 180.0, 180.0 * M_PI / 180.0);
            cr.close_path();
            cr.stroke();
        }

    protected:
        virtual bool DoGetEquals(const Window *other) const override
        {
            auto otherOurs = dynamic_cast<const decltype(this)>(other);
            if (!otherOurs) return false;
            if(!Window::DoGetEquals(other)) return false;
            if(m_Color != otherOurs->m_Color) return false;
            return true;
        }

    private:
        utils::TFloatColor m_Color;
    };

    class TDrawbarWindow : public simplegui::Window
    {
    public:
        TDrawbarWindow(Window *parent, const utils::TIntRect &rect, const utils::TFloatColor &color, int position) : Window(parent, rect)
        {
            position = std::clamp(position, 0, 8);
            int buttonheight = rect.Height() / 3;
            int buttontop_full = rect.Height() - buttonheight;
            int buttontop = 0 + position * buttontop_full / 8;
            int fontsize = buttontop_full / 10;
            int whitewidth = 9 * rect.Width() / 10;
            int blackwidth = rect.Width() / 3;
            auto whitebox = AddChild<simplegui::PlainWindow>(utils::TIntRect::FromTopLeftAndSize({rect.Width() / 2 - whitewidth / 2, 0}, {whitewidth, buttontop}), utils::TFloatColor(1, 1, 1, 1));
            auto blackbox = whitebox->AddChild<simplegui::PlainWindow>(utils::TIntRect::FromTopLeftAndSize({whitebox->Rectangle().Width() / 2 - blackwidth / 2, 0}, {blackwidth, whitebox->Rectangle().Height()}), utils::TFloatColor(0, 0, 0, 1));
            for(int p = 1; p <= position; p++)
            {
                int boxtop = (position - p) * buttontop_full / 8;
                int boxbottom = (position - p + 1) * buttontop_full / 8;
                blackbox->AddChild<simplegui::TextWindow>(utils::TIntRect::FromTopLeftAndSize({0, boxtop}, {blackwidth, boxbottom - boxtop}), std::to_string(p), utils::TFloatColor(1, 1, 1, 1), fontsize, simplegui::TextWindow::THalign::Center);
            }
            AddChild<TDrawbarKnob>(utils::TIntRect::FromTopLeftAndSize({0, buttontop}, {rect.Width(), buttonheight}), color);
        }

    protected:
        virtual bool DoGetEquals(const Window *other) const override
        {
            auto otherOurs = dynamic_cast<const decltype(this)>(other);
            if (!otherOurs) return false;
            if(!Window::DoGetEquals(other)) return false;
            return true;
        }
    };

    std::optional<size_t> TGuiState::GetSelectedPresetIndex() const
    {
        auto now = std::chrono::steady_clock::now();
        if(m_SelectedPresetIndex && (now < m_LastSelectedPresetChange + sDiscardSelectedPresetIndexAfter))
        {
            return m_SelectedPresetIndex;
        }
        return ActivePresetIndexForFocusedPart();
    }

    std::optional<std::chrono::steady_clock::time_point> TGuiState::NextScreenUpdateNeeded() const
    {
        auto now = std::chrono::steady_clock::now();
        std::optional<std::chrono::steady_clock::time_point> result;
        if(m_SelectedPresetIndex)
        {
            auto p1 = m_LastSelectedPresetChange + sDiscardSelectedPresetIndexAfter;
            if(p1 > now)
            {
                if( (!result) || (*result > p1) )
                {
                    result = p1;
                }
            }
        }
        return result;
    }

    void TGuiState::SetFocusedPart(std::optional<size_t> focusedPart)
    {
        if(focusedPart)
        {
            if(m_EngineData.Project().Parts().empty())
            {
                focusedPart = std::nullopt;
            }
            else
            {
                focusedPart = std::min(*focusedPart, m_EngineData.Project().Parts().size() - 1);
            }
        }
        else
        {
            if(!m_EngineData.Project().Parts().empty())
            {
                focusedPart = 0;
            }
        }
        if(focusedPart != m_FocusedPart)
        {
            m_SelectedPresetIndex = std::nullopt;
            m_FocusedPart = focusedPart;
        }
    }

    std::optional<size_t> TGuiState::ActivePresetIndexForFocusedPart() const
    {
        if(!m_FocusedPart)
        {
            return std::nullopt;
        }
        return m_EngineData.Project().Parts().at(*m_FocusedPart).ActivePresetIndex();
    }

    void TGuiState::SetSelectedPresetIndex(std::optional<size_t> index)
    {
        m_SelectedPresetIndex = index;
        m_LastSelectedPresetChange = std::chrono::steady_clock::now();
    }

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
        auto prevactivepresetindex = ActivePresetIndexForFocusedPart();
        m_EngineData = engineData;
        SetFocusedPart(m_FocusedPart); // this will bring it in range if necessary
        if(m_SelectedPresetIndex && (*m_SelectedPresetIndex >= m_EngineData.Project().Presets().size()))
        {
            m_SelectedPresetIndex = std::nullopt;
        }
        if(prevactivepresetindex != ActivePresetIndexForFocusedPart())
        {
            m_SelectedPresetIndex = std::nullopt;
        }
    }

    void Gui::SetGuiState(TGuiState &&state)
    {
        m_GuiState1 = std::move(state);
        RefreshLcd();
        RefreshLeds();
        if(!m_GuiStateNoRecurse)
        {
            m_GuiStateNoRecurse = true;
            utils::finally finally([this](){m_GuiStateNoRecurse = false;});
            if(m_Engine.Data() != GuiState().EngineData())
            {
                auto data = GuiState().EngineData();
                m_Engine.SetData(std::move(data));
            }
        }
    }

    Gui::Gui(std::pair<int, int> vidPid, engine::Engine &engine) : m_Engine(engine), m_Hid(vidPid, [this](Hid::TButtonIndex button, int delta) { OnButton(button, delta); }), m_OnProjectChanged {m_Engine.OnDataChanged(), [this](){OnDataChanged();}}, m_OnOutputLevelUpdate(m_Engine.RtProcessor().OnOutputLevelChange(), [this](){OnOutputLevelChanged();})
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
        if( ((button >= Hid::TButtonIndex::TouchLcdRotary0) && (button <= Hid::TButtonIndex::TouchLcdRotary7)) || (button == Hid::TButtonIndex::TouchRotary))
        {
            size_t rotaryindex = (button == Hid::TButtonIndex::TouchRotary)? 8 : ((size_t)((int)button - (int)Hid::TButtonIndex::TouchLcdRotary0));
            auto guistate = GuiState();
            guistate.m_TouchingRotary[rotaryindex] = delta > 0;
            SetGuiState(std::move(guistate));
        }
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
            guistate.m_Shift = false;
            SetGuiState(std::move(guistate));
        }
        if( (button == Hid::TButtonIndex::Midi) && (delta > 0) )
        {
            auto guistate = GuiState();
            guistate.m_Mode = TGuiState::TMode::Midi;
            guistate.m_Shift = false;
            SetGuiState(std::move(guistate));
        }
        if( (button == Hid::TButtonIndex::Plugin) && (delta > 0) )
        {
            auto guistate = GuiState();
            guistate.m_Mode = TGuiState::TMode::Controller;
            guistate.m_Shift = false;
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
                newguistate.m_Shift = false;
                size_t newfocusedpart;
                if(newguistate.m_FocusedPart)
                {
                    newfocusedpart = (*newguistate.m_FocusedPart) + 1;
                    if(newfocusedpart >= numparts)
                    {
                        newfocusedpart = 0;
                    }
                }
                else
                {
                    newfocusedpart = 0;
                }
                newguistate.SetFocusedPart(newfocusedpart);
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
            if( ((button == Hid::TButtonIndex::Left) || (button == Hid::TButtonIndex::Right)) && (delta > 0) )
            {
                size_t newpresetPage = GuiState().m_QuickPresetPage;
                if(button == Hid::TButtonIndex::Left)
                {
                    if(newpresetPage > 0) newpresetPage--;
                }
                else if(button == Hid::TButtonIndex::Right)
                {
                    if(newpresetPage < (sNumPresetPages-1) ) newpresetPage++;
                }
                if(newpresetPage != GuiState().m_QuickPresetPage)
                {
                    auto newguistate = GuiState();
                    newguistate.m_QuickPresetPage = newpresetPage;
                    SetGuiState(std::move(newguistate));
                }
            }
            if(button == Hid::TButtonIndex::BigRotary)
            {
                auto v = delta;
                auto numpresets = GuiState().EngineData().Project().Presets().size();
                if(numpresets > 0)
                {
                    if(v != 0)
                    {
                        size_t newselected = 0;
                        auto oldselected = GuiState().GetSelectedPresetIndex();
                        if(oldselected)
                        {
                            int newselected_i = (int)(*oldselected) + v;
                            if(newselected_i < 0) newselected_i = 0;
                            if(newselected_i >= (int)numpresets) newselected_i = numpresets - 1;
                            newselected = (size_t)newselected_i;
                        }
                        {
                            auto newguistate = GuiState();
                            newguistate.SetSelectedPresetIndex(newselected);
                            SetGuiState(std::move(newguistate));
                        }
                    }
                }
            }
            if( (button >= Hid::TButtonIndex::LcdRotary0) && (button <= Hid::TButtonIndex::LcdRotary7) )
            {
                int rotaryindex = (int)button - (int)Hid::TButtonIndex::LcdRotary0;
                auto partrange = GuiState().VisiblePartVolumeSliders();
                int numvisibleparts = partrange.second - partrange.first;
                if(rotaryindex >= 7-numvisibleparts)
                {
                    int partindex_i;
                    if(rotaryindex == 7-numvisibleparts)
                    {
                        partindex_i = -1; // reverb
                    }
                    else
                    {
                        partindex_i = partrange.first + (rotaryindex - (8-numvisibleparts));
                    }
                    auto v = m_VolumeHysteresis.at(rotaryindex).Update(delta);
                    if(v != 0)
                    {
                        double dB = v * 0.5;
                        if(partindex_i < 0)
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
                            auto partindex = (size_t)partindex_i;
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
            }
            if( (button == Hid::TButtonIndex::RotaryClick) && (delta > 0) )
            {
                const auto &project = GuiState().EngineData().Project();
                auto selectedpresetindex = GuiState().GetSelectedPresetIndex();
                if(GuiState().m_FocusedPart && selectedpresetindex)
                {
                    auto newproject = GuiState().EngineData().Project().SwitchToPreset(GuiState().m_FocusedPart.value(), *selectedpresetindex);
                    auto newengindata = GuiState().EngineData().ChangeProject(std::move(newproject));
                    auto newguistate = GuiState();
                    newguistate.SetEngineData(std::move(newengindata));
                    SetGuiState(std::move(newguistate));
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
                        auto presetindex = GuiState().GetSelectedPresetIndex();
                        if(presetindex)
                        {
                            auto newpart = part.ChangeQuickPreset(quickPresetIndex, *presetindex);
                            auto newproject = GuiState().EngineData().Project().ChangePart(focusedpartindex, std::move(newpart));
                            auto newenginedata = GuiState().EngineData().ChangeProject(std::move(newproject));
                            auto newguistate = GuiState();
                            newguistate.SetEngineData(std::move(newenginedata));
                            newguistate.m_Shift = false;
                            SetGuiState(std::move(newguistate));
                        }
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
    }

    Gui::~Gui()
    {
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_AbortRequested = true;
        }
        m_GuiThread.join();
    }

    void Gui::OnOutputLevelChanged()
    {
        auto newoutputlevel = m_Engine.RtProcessor().OutputPeakLevelDb();
        auto newguistate = GuiState();
        if(newguistate.m_OutputLevel != newoutputlevel)
        {
            newguistate.m_OutputLevel = newoutputlevel;
            SetGuiState(std::move(newguistate));
        }
    }

    void Gui::Run()
    {
        // called periodically
        m_Hid.Run();
        auto now = std::chrono::steady_clock::now();
        constexpr auto peaklevelrefreshtime = std::chrono::milliseconds(500);
        auto newguistate = GuiState();
        bool guistatechanged = false;
        if(now >= m_LastPeakLevelUpdate + peaklevelrefreshtime)
        {
            m_LastPeakLevelUpdate = now;
            auto peaklevel = m_Engine.RtProcessor().OutputPeakLevelDb();
            if(GuiState().m_OutputPeakLevel != peaklevel)
            {
                newguistate.m_OutputPeakLevel = peaklevel;
                guistatechanged = true;
            }
        }
        if(guistatechanged)
        {
            SetGuiState(std::move(newguistate));
        }
        if(m_NextScheduledLcdRefresh)
        {
            if(now >= *m_NextScheduledLcdRefresh)
            {
                m_NextScheduledLcdRefresh = std::nullopt;
                RefreshLcd();
            }
        }
    }

    void Gui::RunGuiThread(std::pair<int, int> vidPid)
    {
        Display display(vidPid);
        while(true)
        {
            std::this_thread::sleep_for(2ms);
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
                utils::TIntRegion dirtyregion;
                window->GetUpdateRegion(m_PrevWindow.get(), dirtyregion, {0, 0});
                PaintWindow(display, *window, dirtyregion);
                m_PrevWindow = std::move(window);
                // display.SendPixels(0, 0, Display::sWidth, Display::sHeight);
                dirtyregion = dirtyregion.Intersection(utils::TIntRect::FromSize({Display::sWidth, Display::sHeight}));
                dirtyregion.ForEach([&](const utils::TIntRect &rect) {
                    display.SendPixels(rect.Left(), rect.Top(), rect.Width(), rect.Height());
                });
                auto endtime = std::chrono::steady_clock::now();
                [[maybe_unused]] auto elapsed = endtime - starttime;
                // std::cout << "Painting took " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "ms" << std::endl;
            }
            display.Run();
            //std::this_thread::sleep_for(100ms);
        }
    }

    void Gui::SetWindow(std::unique_ptr<simplegui::Window> window)
    {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_NewWindow = std::move(window);
    }

    void Gui::PaintWindow(Display &display, const simplegui::Window &window, const utils::TIntRegion &dirtyregion)
    {
        uint16_t* pixelbuf = display.DisplayBuffer(0, 0);
        auto surface = Cairo::ImageSurface::create((unsigned char*)pixelbuf, Cairo::Format::FORMAT_RGB16_565, Display::sWidth, Display::sHeight, Display::sDisplayBufferStride);
        auto cr = Cairo::Context::create(surface);
        // fill black:
        cr->set_source_rgb(0, 0, 0);
        cr->paint();
        Cairo::Context *context = cr.operator->();
        window.Paint(*context);
        /*
        // draw dirty regions:
        auto randomcolor = utils::TFloatColor((double)rand() / RAND_MAX, (double)rand() / RAND_MAX, (double)rand() / RAND_MAX);
        for(size_t i = 0; i < dirtyregion.get_num_rectangles(); i++)
        {
            auto rect = dirtyregion.get_rectangle(i);
            cr->set_source_rgba(randomcolor.Red(), randomcolor.Green(), randomcolor.Blue(), randomcolor.Alpha());
            // draw outline with single pixel pen:
            cr->set_line_width(1.0);
            cr->rectangle(rect.x, rect.y, rect.width, rect.height);
            cr->stroke();                    
        }
        */

    }
    void Gui::OnDataChanged()
    {
        if(!m_GuiStateNoRecurse)
        {
            m_GuiStateNoRecurse = true;
            utils::finally finally([this](){m_GuiStateNoRecurse = false;});
            auto newstate = GuiState();
            newstate.SetEngineData(m_Engine.Data());
            SetGuiState(std::move(newstate));
        }
    }
    void Gui::PaintPerformanceWindow(simplegui::Window &window) const
    {
        const auto &project = GuiState().EngineData().Project();

        int lineheight = sFontSize + 2;

        /*
        std::string presetname;
        if(GuiState().m_SelectedPreset < project.Presets().size())
        {
            const auto &presetOrNull = project.Presets()[GuiState().m_SelectedPreset];
            if(presetOrNull)
            {
                presetname = presetOrNull->Name();
            }
        }
        int presetboxheight = 2*lineheight + sLineSpacing + 2;
        auto presetnamebox = window.AddChild<simplegui::PlainWindow>(utils::TIntRect::FromTopLeftAndSize(0, 272-presetboxheight, 120, presetboxheight), utils::TFloatColor(0.2, 0.2, 0.2));

        presetnamebox->AddChild<simplegui::TextWindow>(utils::TIntRect::FromTopLeftAndSize(1, 1, presetnamebox->Width() - 2, lineheight), std::to_string(GuiState().m_SelectedPreset), utils::TFloatColor(1, 1, 1), sFontSize, simplegui::TextWindow::THalign::Left);

        presetnamebox->AddChild<simplegui::TextWindow>(utils::TIntRect::FromTopLeftAndSize(1, 1 + lineheight + sLineSpacing, presetnamebox->Width() - 2, lineheight), presetname, utils::TFloatColor(1, 1, 1), sFontSize, simplegui::TextWindow::THalign::Left);
*/
        {
            // output level:
            int levelmetertop = 80;
            int levelmeterheight = 20;
            int levelmeterleft = 0;
            int levelmeterwidth = 200;

            auto text = dbToText(GuiState().m_OutputPeakLevel);
            auto slidervalue = dbToSliderValue(GuiState().m_OutputLevel);
            window.AddChild<simplegui::TSlider>(utils::TIntRect::FromTopLeftAndSize({levelmeterleft, levelmetertop}, {levelmeterwidth, levelmeterheight}), text, slidervalue, utils::TFloatColor(0.8, 0.8, 0.8));
        }
        {
            // Pager:
            int pagertop = 50;
            int pagerheight = sFontSize + 4;
            int pagerx0 = 0;
            int pagerx1 = pagerx0 + 2 * pagerheight / 3;
            int pagerx2 = pagerx1 + 2 * pagerheight;
            int pagerx3 = pagerx2 + 2 * pagerheight / 3;
            auto pagercolor = utils::TFloatColor(1, 1, 1);
            if(GuiState().m_QuickPresetPage > 0)
            {
                window.AddChild<simplegui::TTriangle>(utils::TIntRect::FromTopLeftAndSize({pagerx0, pagertop}, {pagerx1 - pagerx0, pagerheight}), pagercolor, simplegui::TTriangle::TDirection::Left);
            }
            window.AddChild<simplegui::TextWindow>(utils::TIntRect::FromTopLeftAndSize({pagerx1, pagertop}, {pagerx2 - pagerx1, pagerheight}), std::to_string(GuiState().m_QuickPresetPage + 1), pagercolor, sFontSize, simplegui::TextWindow::THalign::Center);
            if(GuiState().m_QuickPresetPage < (sNumPresetPages - 1))
            {
                window.AddChild<simplegui::TTriangle>(utils::TIntRect::FromTopLeftAndSize({pagerx2, pagertop}, {pagerx3 - pagerx2, pagerheight}), pagercolor, simplegui::TTriangle::TDirection::Right);
            }
        }

        if(GuiState().m_FocusedPart)
        {
            // Preset menu buttons:
            auto partcolor = colorForPart(GuiState().m_FocusedPart.value());
            int quickPresetsBoxHeight = sFontSize + 4;
            int quickPresetHorzPadding = 1;

            auto quickPresetsBar = window.AddChild<simplegui::Window>(utils::TIntRect::FromTopLeftAndSize({0, 0}, {960, quickPresetsBoxHeight}));
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
                    auto boxcolor = GuiState().m_Shift? utils::TFloatColor(0.8, 0.8, 0.8): partcolor;
                    auto quickPresetBox = quickPresetsBar->AddChild<simplegui::PlainWindow>(utils::TIntRect::FromTopLeftAndSize({(int)quickPresetOffset * 120 + quickPresetHorzPadding, 0}, {120 - 2*quickPresetHorzPadding, quickPresetsBoxHeight}), boxcolor);
                    quickPresetBox->AddChild<simplegui::TextWindow>(quickPresetBox->Rectangle().SymmetricalExpand({-1}), presetName, utils::TFloatColor(0, 0, 0), sFontSize, simplegui::TextWindow::THalign::Left);
                }
            }
        }


        {
            // Volume controls:
            bool hasfocusedslider = false;
            utils::TFloatColor focusedslidercolor;
            double focusedslidervalue = 0;
            std::string focusedslidertext;
            int sliderbottom = 272;
            int sliderheight = 30;
            int sliderlabeltop = sliderbottom - sliderheight - lineheight;
            auto sliderrange = GuiState().VisiblePartVolumeSliders();
            int numvolumesliders = sliderrange.second - sliderrange.first;
            
            for(int sliderindex = -1; sliderindex < numvolumesliders; sliderindex++)
            {
                int rotaryindex = 8 - numvolumesliders + sliderindex;
                int sliderleft = rotaryindex * 120 + 5;
                int sliderright = sliderleft + 120 - 10;
                double multiplier;
                utils::TFloatColor slidercolor = utils::TFloatColor(0.7, 0.7, 0.7);
                std::string label;
                if(sliderindex < 0)
                {
                    multiplier = project.Reverb().MixLevel();
                    label = "Reverb";
                }
                else
                {
                    size_t partindex = size_t(sliderrange.first + sliderindex);
                    multiplier = 0;
                    if(project.Parts().size() > partindex)
                    {
                        multiplier = project.Parts().at(partindex).AmplitudeFactor();
                    }
                    slidercolor = colorForPart(partindex);
                    label = project.Parts().at(partindex).Name();
                }
                double slidervalue = multiplierToSliderValue(multiplier);
                auto slidertext = multiplierToText(multiplier);

                window.AddChild<simplegui::TextWindow>(utils::TIntRect::FromTopLeftAndSize({sliderleft, sliderlabeltop}, {sliderright - sliderleft, lineheight}), label, slidercolor, sFontSize, simplegui::TextWindow::THalign::Left);

                window.AddChild<simplegui::TSlider>(utils::TIntRect::FromTopLeftAndSize({sliderleft, sliderbottom - sliderheight}, {sliderright - sliderleft, sliderheight}), slidertext, slidervalue, slidercolor);
                bool isfocused = GuiState().m_TouchingRotary.at(rotaryindex);
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
                auto focusedslider = window.AddChild<simplegui::TSlider>(utils::TIntRect::FromTopLeftAndSize({focusedsliderleft, focusedsliderbottom - focusedsliderheight}, {focusedsliderright - focusedsliderleft, focusedsliderheight}), focusedslidertext, focusedslidervalue, focusedslidercolor);
            }
        }
        {
            // Active preset indicator:
            auto partrange = GuiState().VisiblePartPresetNames();
            int top = lineheight + 20;
            int fontsize = 3 * sFontSize /2;
            int presetlineheight = fontsize + 5;
            int boxwidth = 300;
            int boxleft = 960 - boxwidth;
            for(size_t partindex = (size_t)partrange.first; partindex < (size_t) partrange.second; partindex++)
            {
                auto part = project.Parts().at(partindex);
                auto partcolor = colorForPart(partindex);
                std::string label;
                std::optional<size_t> presetindex = part.ActivePresetIndex();
                if(partindex == GuiState().m_FocusedPart)
                {
                    presetindex = GuiState().GetSelectedPresetIndex();
                }
                bool presetOverridden = part.ActivePresetIndex() != presetindex;
                if(presetindex)
                {
                    if(*presetindex < project.Presets().size())
                    {
                        label = std::to_string(*presetindex) + ": ";
                        const auto &presetOrNull = project.Presets()[*presetindex];
                        if(presetOrNull)
                        {
                            label += presetOrNull->Name();
                        }
                    }
                }
                simplegui::Window *boxwindow = nullptr;
                auto boxrect = utils::TIntRect::FromTopLeftAndSize({boxleft, top + presetlineheight * ((int)partindex - partrange.first)}, {boxwidth, presetlineheight});
                utils::TFloatColor textcolor = partcolor;
                if(partindex == GuiState().m_FocusedPart)
                {
                    auto outerwindow = window.AddChild<simplegui::PlainWindow>(boxrect, partcolor);
                    if(presetOverridden)
                    {
                        boxwindow = outerwindow->AddChild<simplegui::Window>(outerwindow->Rectangle().SymmetricalExpand({-1}));
                        textcolor = utils::TFloatColor(0, 0, 0);
                    }
                    else
                    {
                        boxwindow = outerwindow->AddChild<simplegui::PlainWindow>(outerwindow->Rectangle().SymmetricalExpand({-1}), utils::TFloatColor(0, 0, 0));
                    }
                }
                else
                {
                    boxwindow = window.AddChild<simplegui::Window>(boxrect.SymmetricalExpand({-1}));
                }
                boxwindow->AddChild<simplegui::TextWindow>(boxwindow->Rectangle().SymmetricalExpand({-2, 0}), label, textcolor, fontsize, simplegui::TextWindow::THalign::Left);
            }
        }

        if( (GuiState().m_TouchingRotary[8]) && (!project.Presets().empty()) )
        {
            // Big preset popup selector:
            window.AddChild<simplegui::TListBox>(utils::TIntRect::FromTopLeftAndSize({490, 272/2 - 100}, {400, 272/2+100}), utils::TFloatColor(1,1,1), 25, project.Presets().size(), GuiState().GetSelectedPresetIndex(), GuiState().GetSelectedPresetIndex().value_or(0), [this, &project](size_t index) -> std::string {
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
        auto presetnamebox = window.AddChild<simplegui::PlainWindow>(utils::TIntRect::FromTopLeftAndSize({0, 272-presetboxheight}, {120, presetboxheight}), utils::TFloatColor(0.2, 0.2, 0.2));

        presetnamebox->AddChild<simplegui::TextWindow>(utils::TIntRect::FromTopLeftAndSize({1, 1}, {presetnamebox->Rectangle().Width() - 2, lineheight}), "Program", utils::TFloatColor(1, 1, 1), sFontSize, simplegui::TextWindow::THalign::Left);

        presetnamebox->AddChild<simplegui::TextWindow>(utils::TIntRect::FromTopLeftAndSize({1, 1 + lineheight + sLineSpacing}, {presetnamebox->Rectangle().Width() - 2, lineheight}), std::to_string(GuiState().m_ProgramChange), utils::TFloatColor(1, 1, 1), sFontSize, simplegui::TextWindow::THalign::Left);            
    }
    void Gui::PaintHammondControllerWindow(simplegui::Window &window, size_t part) const
    {
        int lineheight = sFontSize + 2;
        const auto &hammonddata = GuiState().EngineData().HammondData();
        const auto &hammondpart = hammonddata.Part(part);
        int drawbartop = 50;
        int drawbarbottom = 250;
        utils::TFloatColor brown(143/255.0, 69/255.0, 0.0, 1.0);
        utils::TFloatColor darkgrey(0.3, 0.3, 0.3, 1.0);
        utils::TFloatColor lightgrey(0.7, 0.7, 0.7, 1.0);
        
        for(size_t drawbarindex = 0; drawbarindex < 9; drawbarindex++)
        {
            auto drawbarcolor = (drawbarindex < 2)? brown : (((drawbarindex == 4) || (drawbarindex == 6) || (drawbarindex == 7))? darkgrey : lightgrey);
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
            window.AddChild<TDrawbarWindow>(utils::TIntRect::FromTopLeftAndSize({drawbarcenter - drawbarwidth/2, drawbartop}, {drawbarwidth, drawbarbottom - drawbartop}), drawbarcolor, value);
        }
        int menuboxheight = sFontSize + 4;
        int menuhorzpadding = 1;
        auto partcolor = colorForPart(part);
        utils::TFloatColor black(0, 0, 0);
        if(part == 0)
        {
            for(size_t buttonindex = 0; buttonindex < 4; buttonindex++)
            {
                bool value = buttonindex == 0? hammonddata.Percussion() : (buttonindex == 1? hammonddata.PercussionSoft() : (buttonindex == 2? hammonddata.PercussionFast(): hammonddata.Percussion2ndHarmonic()));
                std::string label = buttonindex == 0? "Percussion" : (buttonindex == 1? "Soft" : (buttonindex == 2? "Fast": "2nd"));
                auto menubox = window.AddChild<simplegui::PlainWindow>(utils::TIntRect::FromTopLeftAndSize({(int)buttonindex * 120 + menuhorzpadding, 0}, {120 - 2*menuhorzpadding, menuboxheight}), value? partcolor:black);
                menubox->AddChild<simplegui::TextWindow>(menubox->Rectangle().SymmetricalExpand({-1}), label, value? black:partcolor, sFontSize, simplegui::TextWindow::THalign::Center);
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
        m_NextScheduledLcdRefresh = GuiState().NextScreenUpdateNeeded();
        auto mainwindow = std::make_unique<simplegui::Window>(nullptr, utils::TIntRect::FromTopLeftAndSize({0, 0}, {Display::sWidth, Display::sWidth}));
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

        m_Hid.SetLed(Hid::TButtonIndex::Left, (GuiState().m_Mode == TGuiState::TMode::Performance) && (GuiState().m_QuickPresetPage > 0) ? 2:0);
        m_Hid.SetLed(Hid::TButtonIndex::Right, (GuiState().m_Mode == TGuiState::TMode::Performance) && (GuiState().m_QuickPresetPage < (sNumPresetPages-1)) ? 2:0);

        if(GuiState().m_Mode == TGuiState::TMode::Midi)
        {
            for(size_t btnIndex = 0; btnIndex < 8; btnIndex++)
            {
                m_Hid.SetLed(Hid::TButtonIndex(int(Hid::TButtonIndex::Menu0) + (int)btnIndex), 0);
            }
        }
        else if(GuiState().m_Mode == TGuiState::TMode::Performance)
        {
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
