#ifndef _AL_EFFECT_H_
#define _AL_EFFECT_H_

#include "alMain.h"


struct ALeffect;

typedef union ALeffectProps {
    struct Reverb {
        // Shared Reverb Properties
        ALfloat density;
        ALfloat diffusion;
        ALfloat gain;
        ALfloat gain_hf;
        ALfloat decay_time;
        ALfloat decay_hf_ratio;
        ALfloat reflections_gain;
        ALfloat reflections_delay;
        ALfloat late_reverb_gain;
        ALfloat late_reverb_delay;
        ALfloat air_absorption_gain_hf;
        ALfloat room_rolloff_factor;
        ALboolean decay_hf_limit;

        // Additional EAX Reverb Properties
        ALfloat gain_lf;
        ALfloat decay_lf_ratio;
        ALfloat reflections_pan[3];
        ALfloat late_reverb_pan[3];
        ALfloat echo_time;
        ALfloat echo_depth;
        ALfloat modulation_time;
        ALfloat modulation_depth;
        ALfloat hf_reference;
        ALfloat lf_reference;
    } reverb;

    struct Chorus {
        ALint waveform;
        ALint phase;
        ALfloat rate;
        ALfloat depth;
        ALfloat feedback;
        ALfloat delay;
    } chorus;

    struct ReverbCompressor {
        ALboolean on_off;
    } compressor;

    struct Distortion {
        ALfloat edge;
        ALfloat gain;
        ALfloat lowpass_cutoff;
        ALfloat eq_center;
        ALfloat eq_bandwidth;
    } distortion;

    struct Echo {
        ALfloat delay;
        ALfloat lr_delay;

        ALfloat damping;
        ALfloat feedback;

        ALfloat spread;
    } echo;

    struct Equalizer {
        ALfloat low_cutoff;
        ALfloat low_gain;
        ALfloat mid1_center;
        ALfloat mid1_gain;
        ALfloat mid1_width;
        ALfloat mid2_center;
        ALfloat mid2_gain;
        ALfloat mid2_width;
        ALfloat high_cutoff;
        ALfloat high_gain;
    } equalizer;

    struct Flanger {
        ALint waveform;
        ALint phase;
        ALfloat rate;
        ALfloat depth;
        ALfloat feedback;
        ALfloat delay;
    } flanger;

    struct Modulator {
        ALfloat frequency;
        ALfloat high_pass_cutoff;
        ALint waveform;
    } modulator;

    struct Dedicated {
        ALfloat gain;
    } dedicated;
} ALeffectProps;

typedef struct ALeffect {
    // Effect type (AL_EFFECT_NULL, ...)
    ALenum type;

    ALeffectProps props;
} ALeffect;


class IEffect
{
public:
    IEffect(
        const IEffect& that) = delete;

    IEffect& operator=(
        const IEffect& that) = delete;

    virtual ~IEffect();


    ALfloat (*out_buffer)[BUFFERSIZE];
    ALsizei out_channels;


    void construct();

    void destruct();

    ALboolean update_device(
        ALCdevice* device);

    void update(
        const ALCdevice* device,
        const struct ALeffectslot* slot,
        const union ALeffectProps *props);

    void process(
        ALsizei samplesToDo,
        const ALfloat(*samplesIn)[BUFFERSIZE],
        ALfloat(*samplesOut)[BUFFERSIZE],
        ALsizei numChannels);


protected:
    IEffect();


    virtual void do_construct() = 0;

    virtual void do_destruct() = 0;

    virtual ALboolean do_update_device(
        ALCdevice* device) = 0;

    virtual void do_update(
        const ALCdevice* device,
        const struct ALeffectslot* slot,
        const union ALeffectProps *props) = 0;

    virtual void do_process(
        ALsizei samplesToDo,
        const ALfloat(*samplesIn)[BUFFERSIZE],
        ALfloat(*samplesOut)[BUFFERSIZE],
        ALsizei numChannels) = 0;
}; // IEffect


ALenum InitEffect(ALeffect *effect);


#endif
