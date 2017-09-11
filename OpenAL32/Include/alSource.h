#ifndef _AL_SOURCE_H_
#define _AL_SOURCE_H_


#include <memory>
#include "alu.h"


struct ALsource
{
    // Direct filter and auxiliary send info.

    struct Direct
    {
        float gain;
        float gain_hf;
        float hf_reference;
        float gain_lf;
        float lf_reference;
    }; // Direct

    struct Send
    {
        struct EffectSlot *slot;
        float gain;
        float gain_hf;
        float hf_reference;
        float gain_lf;
        float lf_reference;
    }; // Send

    using SendUPtr = std::unique_ptr<Send>;

    Direct direct;
    SendUPtr send;

    // Source state (initial, playing, paused, or stopped)
    int state;
}; // ALsource


void update_all_source_props(ALCdevice* device);


#endif
