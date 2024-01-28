#include "conspiracy.h"

/*
Pads:
ch 1: (note - 36)

*/

namespace conspiracy {
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
                        std::cout << "presetNumber=" << *presetNumber << std::endl;
                        const auto &project = Engine().Project();
                        if(project.FocusedPart())
                        {
                            auto newpart = project.Parts().at(*project.FocusedPart()).ChangeActivePresetIndex(*presetNumber);
                            auto newproject = project.ChangePart(*project.FocusedPart(), std::move(newpart));
                            Engine().SetProject(std::move(newproject));
                        }

                    }
                }
            }
        }

    }
} // namespace