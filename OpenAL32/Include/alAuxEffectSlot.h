#ifndef _AL_AUXEFFECTSLOT_H_
#define _AL_AUXEFFECTSLOT_H_


#include "alEffect.h"


struct ALeffectslot;


#define MAX_EFFECT_CHANNELS (4)


struct ALeffectslotArray {
    ALsizei count;
    struct ALeffectslot *slot[];
};


struct ALeffectslotProps {
    ALenum type;
    ALeffectProps props;

    IEffect *state;

    struct ALeffectslotProps* next;
};


typedef struct ALeffectslot {
    struct Effect {
        ALenum type;
        ALeffectProps props;

        IEffect *state;
    } effect;

    unsigned int ref;

    struct ALeffectslotProps* update;
    struct ALeffectslotProps* free_list;

    struct Params {
        ALenum effect_type;
        IEffect *effect_state;
    } params;

    ALsizei num_channels;
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
    ALfloat wet_buffer[MAX_EFFECT_CHANNELS][BUFFERSIZE];
} ALeffectslot;

ALenum InitEffectSlot(ALeffectslot *slot);
void DeinitEffectSlot(ALeffectslot *slot);
void UpdateEffectSlotProps(ALeffectslot *slot);
void UpdateAllEffectSlotProps(ALCcontext *context);


ALenum InitializeEffect(ALCdevice *Device, ALeffectslot *EffectSlot, ALeffect *effect);


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
