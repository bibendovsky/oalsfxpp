#ifndef _AL_SOURCE_H_
#define _AL_SOURCE_H_


#include <memory>
#include "alu.h"


struct ALsource
{
    struct Base
    {
        float gain_;
        float gain_hf_;
        float hf_reference_;
        float gain_lf_;
        float lf_reference_;
    }; // Base

    struct Send : Base
    {
        EffectSlot* effect_slot_;
    }; // Send

    using SendUPtr = std::unique_ptr<Send>;

    Base direct_;
    SendUPtr send_;

    // Source state (initial, playing, paused, or stopped)
    int state_;
}; // ALsource


void update_all_source_props(
    ALCdevice* device);


#endif
