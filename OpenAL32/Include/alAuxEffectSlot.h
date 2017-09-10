#ifndef _AL_AUXEFFECTSLOT_H_
#define _AL_AUXEFFECTSLOT_H_


#include <memory>
#include "alEffect.h"


constexpr auto max_effect_channels = 4;


struct EffectSlot
{
    Effect effect_;
    EffectState* effect_state_;
    bool is_props_updated_;

    int channel_count_;
    BFChannelConfig channel_map_[max_effect_channels];

    // Wet buffer configuration is ACN channel order with N3D scaling:
    // * Channel 0 is the unattenuated mono signal.
    // * Channel 1 is OpenAL -X
    // * Channel 2 is OpenAL Y
    // * Channel 3 is OpenAL -Z
    // Consequently, effects that only want to work with mono input can use
    // channel 0 by itself. Effects that want multichannel can process the
    // ambisonics signal and make a B-Format pan (ComputeFirstOrderGains) for
    // first-order device output (FOAOut).
    SampleBuffers wet_buffer_;


    EffectSlot();

    EffectSlot(
        const EffectSlot& that) = delete;

    EffectSlot& operator=(
        const EffectSlot& that) = delete;

    ~EffectSlot();

    void initialize();

    void uninitialize();

    void initialize_effect(
        ALCdevice* device);
}; // EffectSlot


#endif
