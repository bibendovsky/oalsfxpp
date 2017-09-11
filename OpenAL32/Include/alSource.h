#ifndef _AL_SOURCE_H_
#define _AL_SOURCE_H_


#include <memory>
#include "alu.h"


struct ALsource
{
    // Direct filter and auxiliary send info.

    struct Direct
    {
        float gain_;
        float gain_hf_;
        float hf_reference_;
        float gain_lf_;
        float lf_reference_;
    }; // Direct

    struct Send
    {
        EffectSlot* effect_slot_;
        float gain_;
        float gain_hf_;
        float hf_reference_;
        float gain_lf_;
        float lf_reference_;
    }; // Send

    using SendUPtr = std::unique_ptr<Send>;

    Direct direct_;
    SendUPtr send_;

    // Source state (initial, playing, paused, or stopped)
    int state_;
}; // ALsource


void update_all_source_props(
    ALCdevice* device);


#endif
