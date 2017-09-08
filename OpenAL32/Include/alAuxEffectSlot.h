#ifndef _AL_AUXEFFECTSLOT_H_
#define _AL_AUXEFFECTSLOT_H_


#include "alEffect.h"


constexpr auto max_effect_channels = 4;


struct ALeffectslotProps
{
    int type;
    ALeffectProps props;
    IEffect* state;
}; // ALeffectslotProps


struct ALeffectslot
{
    struct Effect
    {
        int type;
        ALeffectProps props;
        IEffect* state;
    }; // Effect

    struct Params
    {
        int effect_type;
        IEffect* effect_state;
    }; // Params


    Effect effect;
    ALeffectslotProps* update;
    ALeffectslotProps* props;
    Params params;

    int num_channels;
    BFChannelConfig chan_map[max_effect_channels];

    // Wet buffer configuration is ACN channel order with N3D scaling:
    // * Channel 0 is the unattenuated mono signal.
    // * Channel 1 is OpenAL -X
    // * Channel 2 is OpenAL Y
    // * Channel 3 is OpenAL -Z
    // Consequently, effects that only want to work with mono input can use
    // channel 0 by itself. Effects that want multichannel can process the
    // ambisonics signal and make a B-Format pan (ComputeFirstOrderGains) for
    // first-order device output (FOAOut).
    SampleBuffers wet_buffer;


    ALeffectslot()
        :
        wet_buffer{SampleBuffers::size_type{max_effect_channels}}
    {
    }
}; // ALeffectslot


void init_effect_slot(ALeffectslot* slot);
void deinit_effect_slot(ALeffectslot* slot);
void update_effect_slot_props(ALeffectslot* slot);
void update_all_effect_slot_props(ALCdevice* device);
void initialize_effect(ALCdevice* device, ALeffectslot* effect_slot, ALeffect* effect);


template<typename T>
IEffect* create_effect()
{
    auto result = static_cast<IEffect*>(new T{});
    result->construct();
    return result;
}

inline void destroy_effect(
    IEffect*& effect)
{
    if (!effect)
    {
        return;
    }

    effect->destruct();
    delete effect;
    effect = nullptr;
}


#endif
