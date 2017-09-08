#ifndef _AL_SOURCE_H_
#define _AL_SOURCE_H_


#include <memory>
#include "alu.h"


constexpr auto MAX_SENDS = 1;
constexpr auto DEFAULT_SENDS = 1;


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
        struct ALeffectslot *slot;
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


void UpdateAllSourceProps(ALCcontext *context);


#endif
