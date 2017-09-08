#ifndef _AL_AUXEFFECTSLOT_H_
#define _AL_AUXEFFECTSLOT_H_


#include "alEffect.h"


constexpr auto MAX_EFFECT_CHANNELS = 4;


struct ALeffectslotProps {
    int type;
    ALeffectProps props;
    IEffect* state;
}; // ALeffectslotProps


struct ALeffectslot
{
    struct Effect {
        int type;
        ALeffectProps props;
        IEffect* state;
    }; // Effect

    struct Params {
        int effect_type;
        IEffect* effect_state;
    }; // Params


    Effect effect;
    ALeffectslotProps* update;
    ALeffectslotProps* props;
    Params params;

    int num_channels;
    BFChannelConfig chan_map[MAX_EFFECT_CHANNELS];

    /* Wet buffer configuration is ACN channel order with N3D scaling:
     * * Channel 0 is the unattenuated mono signal.
     * * Channel 1 is OpenAL -X
     * * Channel 2 is OpenAL Y
     * * Channel 3 is OpenAL -Z
     * Consequently, effects that only want to work with mono input can use
     * channel 0 by itself. Effects that want multichannel can process the
     * ambisonics signal and make a B-Format pan (ComputeFirstOrderGains) for
     * first-order device output (FOAOut).
     */
    SampleBuffers wet_buffer;


    ALeffectslot()
        :
        wet_buffer{SampleBuffers::size_type{MAX_EFFECT_CHANNELS}}
    {
    }
}; // ALeffectslot


int InitEffectSlot(ALeffectslot* slot);
void DeinitEffectSlot(ALeffectslot* slot);
void UpdateEffectSlotProps(ALeffectslot* slot);
void UpdateAllEffectSlotProps(ALCdevice* device);
int InitializeEffect(ALCdevice* Device, ALeffectslot* EffectSlot, ALeffect* effect);


template<typename T>
IEffect* create_effect()
{
    IEffect* result = new T{};
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
