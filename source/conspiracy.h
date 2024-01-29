#pragma once
#include "engine.h"

namespace conspiracy {
    class Controller : public engine::TController
    {
    public:
        Controller(engine::Engine &engine) : TController(engine, "conspiracy")
        {
        }
    private:
        void OnMidiIn(const midi::TMidiOrSysexEvent &event) override;
        void OnProjectChanged(const project::Project &prevProject) override;
        void SendCurrentPadColorPresetIndex(size_t presetindex) const;
        void SendCurrentPadColorFocusedPart(size_t partindex) const;
    };
} // namespace