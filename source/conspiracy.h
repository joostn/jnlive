#pragma once
#include "engine.h"




namespace conspiracy {
    class Controller : public engine::TController
    {
    public:
        Controller(engine::Engine &engine) : TController(engine, "conspiracy_in")
        {
        }
        void OnMidiIn(const midi::TMidiOrSysexEvent &event) override;
    };
} // namespace