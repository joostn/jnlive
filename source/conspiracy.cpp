#include "conspiracy.h"

/*
Pads: ch 1: 36 - 60
F1-F10: ch 1: cc 20-29
sliders: s1-s6: ch0-5: cc 7
*/

/*
    let colormap = [0,  // off
        3,  // 001 blue
        2,  // 010 green
        26,  // 011 cyan
        1,  // 100 red
        25,  // 101 purple
        8,  // 110 yellow
        27,  // 111 white
    ];
*/
namespace conspiracy {

    const std::array<int, 3> cKeyboardcolors { 4, // red
        2, // green 
        6, // yellow
    };
    const std::array<int, 8>  cColormap { 4,  // off
        3,  // 001 blue
        2,  // 010 green
        26,  // 011 cyan
        1,  // 100 red
        25,  // 101 purple
        8,  // 110 yellow
        27,  // 111 white
    };

    std::optional<int> noteNumber2PadNumber(int note)
    {
        int padnumber_upsidedown = note - 36;
        if( (padnumber_upsidedown < 0) || (padnumber_upsidedown >= 25 ) )
        {
            return std::nullopt;
        }
        int row = padnumber_upsidedown / 5;
        int col = padnumber_upsidedown % 5;
        int padnumber = (4 - row) * 5 + col;
        return padnumber;
    }

    void Controller::OnMidiIn(const midi::TMidiOrSysexEvent &event)
    {
        std::cout << event.ToDebugString() << std::endl;
        if(!event.IsSysex())
        {
            const auto &simpleEvent = event.GetSimpleEvent();
            if(simpleEvent.type() == midi::SimpleEvent::Type::NoteOn)
            {
                if(simpleEvent.Channel() == 1)
                {
                    auto presetNumber = noteNumber2PadNumber(simpleEvent.NoteNumber());
                    if(presetNumber)
                    {
                        if(Project().FocusedPart())
                        {
                            Engine().SwitchFocusedPartToPreset((size_t)*presetNumber);
/*
                            auto prevpresetindex = Project().Parts().at(*Project().FocusedPart()).ActivePresetIndex();
                            auto newpart = Project().Parts().at(*Project().FocusedPart()).ChangeActivePresetIndex(*presetNumber);
                            auto newproject = Project().ChangePart(*Project().FocusedPart(), std::move(newpart));
                            SetProject(std::move(newproject));
                            if( (prevpresetindex) && (*prevpresetindex < 25) )
                            {
                                SendCurrentPadColorPresetIndex(*prevpresetindex);
                            }
*/
                        }
                    }
                }
            }
            else if(simpleEvent.type() == midi::SimpleEvent::Type::NoteOff)
            {
                if(simpleEvent.Channel() == 1)
                {
                    auto presetNumber = noteNumber2PadNumber(simpleEvent.NoteNumber());
                    if(presetNumber)
                    {
                        // pad is released
                        SendCurrentPadColorPresetIndex(*presetNumber);
                    }
                }
            }
            else if(simpleEvent.type() == midi::SimpleEvent::Type::ControlChange)
            {
                if(simpleEvent.Channel() == 1)
                {
                    if( (simpleEvent.ControlNumber() >= 20) && (simpleEvent.ControlNumber() < 30) )
                    {
                        int Fkey = simpleEvent.ControlNumber() - 19;
                        if(simpleEvent.ControlValue() > 64)
                        {
                            // F key is pressed
                            if( (Fkey == 1) || (Fkey == 5) )
                            {
                                size_t focusedpart = (Fkey == 1) ? 0 : 1;
                                auto newproject = Project().Change(focusedpart, Project().ShowUi());
                                SetProject(std::move(newproject));
                            }
                            if(Fkey == 9)
                            {
                                auto newproject = Project().Change(Project().FocusedPart(), !Project().ShowUi());
                                SetProject(std::move(newproject));
                            }
                        }
                        else
                        {
                            // F key is released
                            if( (Fkey == 1) || (Fkey == 5) )
                            {
                                size_t focusedpart = (Fkey == 1) ? 0 : 1;
                                SendCurrentPadColorFocusedPart(focusedpart);
                            }
                        }
                    }
                }
                if(simpleEvent.ControlNumber() == 7)
                {
                    if(simpleEvent.Channel() < 6)
                    {
                        int slidernum = simpleEvent.Channel() + 1;
                        if( (slidernum == 2) || (slidernum == 3) )
                        {
                            size_t partindex = (slidernum == 2) ? 0 : 1;
                            auto newpart = Project().Parts().at(partindex).ChangeAmplitudeFactor(simpleEvent.ControlValue() / 127.0f);
                            auto newproject = Project().ChangePart(partindex, std::move(newpart));
                            SetProject(std::move(newproject));
                        }
                        if(slidernum == 5)
                        {
                            auto reverb = Project().Reverb().ChangeMixLevel(simpleEvent.ControlValue() / 127.0f);
                            auto newproject = Project().ChangeReverb(std::move(reverb));
                            SetProject(std::move(newproject));
                        }
                    }
                }
                if(simpleEvent.ControlNumber() == 10)
                {
                    if(simpleEvent.Channel() == 0)
                    {
                        auto event = midi::SimpleEvent::NoteOn(0, 24, simpleEvent.ControlValue());
                        SendMidi(event);
                    }
                }
            }
        }
    }


    void Controller::OnProjectChanged(const project::Project &prevProject)
    {
        std::optional<size_t> prevPresetIndex;
        if(prevProject.FocusedPart())
        {
            prevPresetIndex = prevProject.Parts().at(*prevProject.FocusedPart()).ActivePresetIndex();
        }
        std::optional<size_t> newPresetIndex;
        if(Project().FocusedPart())
        {
            newPresetIndex = Project().Parts().at(*Project().FocusedPart()).ActivePresetIndex();
        }
        if( (prevPresetIndex != newPresetIndex) || (prevProject.FocusedPart() != Project().FocusedPart()) )
        {
            if(prevPresetIndex)
            {
                SendCurrentPadColorPresetIndex(*prevPresetIndex);
            }
            if(newPresetIndex && (newPresetIndex != prevPresetIndex))
            {
                SendCurrentPadColorPresetIndex(*newPresetIndex);
            }
        }
        if(prevProject.FocusedPart() != Project().FocusedPart())
        {
            if(prevProject.FocusedPart())
            {
                SendCurrentPadColorFocusedPart(*prevProject.FocusedPart());
            }
            if(Project().FocusedPart())
            {
                SendCurrentPadColorFocusedPart(*Project().FocusedPart());
            }
        }
    }

    void Controller::SendCurrentPadColorPresetIndex(size_t presetindex) const
    {
        if(presetindex >= 25) return; // invisible
        int color = 0;
        if(Project().FocusedPart())
        {
            auto currentPresetNumber = Project().Parts().at(*Project().FocusedPart()).ActivePresetIndex();
            if(currentPresetNumber == presetindex)
            {
                color = cKeyboardcolors.at(*Project().FocusedPart());
            }
        }
        int notenumber = (int)presetindex;
        int velocity = cColormap.at(color);
        auto event = midi::SimpleEvent::NoteOn(0, notenumber, velocity);
        SendMidi(event);
    }

    void Controller::SendCurrentPadColorFocusedPart(size_t partindex) const
    {
        if(partindex >= 2) return; // invisible
        int color = 0;
        if(Project().FocusedPart() == partindex)
        {
            color = cKeyboardcolors.at(*Project().FocusedPart());
        }
        int notenumber = (partindex == 0) ? 25 : 29;
        int velocity = cColormap.at(color);
        auto event = midi::SimpleEvent::NoteOn(0, notenumber, velocity);
        SendMidi(event);
    }
} // namespace