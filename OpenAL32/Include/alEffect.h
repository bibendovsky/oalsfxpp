#ifndef _AL_EFFECT_H_
#define _AL_EFFECT_H_

#include <new>
#include <vector>
#include "alMain.h"


using EffectSampleBuffer = std::vector<float>;


union EffectProps
{
    using Pan = float[3];


    struct Reverb
    {
        // Shared reverb properties
        float density;
        float diffusion;
        float gain;
        float gain_hf;
        float decay_time;
        float decay_hf_ratio;
        float reflections_gain;
        float reflections_delay;
        float late_reverb_gain;
        float late_reverb_delay;
        float air_absorption_gain_hf;
        float room_rolloff_factor;
        bool decay_hf_limit;

        // Additional EAX reverb properties
        float gain_lf;
        float decay_lf_ratio;
        Pan reflections_pan;
        Pan late_reverb_pan;
        float echo_time;
        float echo_depth;
        float modulation_time;
        float modulation_depth;
        float hf_reference;
        float lf_reference;
    }; // Reverb

    struct Chorus
    {
        int waveform;
        int phase;
        float rate;
        float depth;
        float feedback;
        float delay;
    }; // Chorus

    struct Compressor
    {
        bool on_off;
    }; // Compressor

    struct Distortion
    {
        float edge;
        float gain;
        float lowpass_cutoff;
        float eq_center;
        float eq_bandwidth;
    }; // Distortion

    struct Echo
    {
        float delay;
        float lr_delay;

        float damping;
        float feedback;

        float spread;
    }; // Echo

    struct Equalizer
    {
        float low_cutoff;
        float low_gain;
        float mid1_center;
        float mid1_gain;
        float mid1_width;
        float mid2_center;
        float mid2_gain;
        float mid2_width;
        float high_cutoff;
        float high_gain;
    }; // Equalizer

    struct Flanger
    {
        int waveform;
        int phase;
        float rate;
        float depth;
        float feedback;
        float delay;
    }; // Flanger

    struct Modulator
    {
        float frequency;
        float high_pass_cutoff;
        int waveform;
    }; // Modulator

    struct Dedicated
    {
        float gain;
    }; // Dedicated


    Reverb reverb;
    Chorus chorus;
    Compressor compressor;
    Distortion distortion;
    Echo echo;
    Equalizer equalizer;
    Flanger flanger;
    Modulator modulator;
    Dedicated dedicated;
}; // EffectProps


struct Effect
{
    // Effect type (AL_EFFECT_NULL, ...)
    int type_;

    EffectProps props_;


    Effect();

    void initialize(
        const int type = AL_EFFECT_NULL);
}; // Effect


class EffectState
{
public:
    EffectState(
        const EffectState& that) = delete;

    EffectState& operator=(
        const EffectState& that) = delete;

    virtual ~EffectState();


    SampleBuffers* out_buffer;
    int out_channels;


    void construct();

    void destruct();

    void update_device(
        ALCdevice* device);

    void update(
        ALCdevice* device,
        const EffectSlot* slot,
        const EffectProps *props);

    void process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count);


    static void destroy(
        EffectState*& effect_state);


protected:
    EffectState();


    virtual void do_construct() = 0;

    virtual void do_destruct() = 0;

    virtual void do_update_device(
        ALCdevice* device) = 0;

    virtual void do_update(
        ALCdevice* device,
        const EffectSlot* slot,
        const EffectProps *props) = 0;

    virtual void do_process(
        const int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) = 0;
}; // EffectState


class EffectStateFactory
{
public:
    static EffectState* create_by_type(
        const int type);


private:
    static EffectState* create_chorus();
    static EffectState* create_compressor();
    static EffectState* create_dedicated();
    static EffectState* create_distortion();
    static EffectState* create_echo();
    static EffectState* create_equalizer();
    static EffectState* create_flanger();
    static EffectState* create_modulator();
    static EffectState* create_null();
    static EffectState* create_reverb();


    template<typename T>
    static EffectState* create()
    {
        auto result = static_cast<EffectState*>(new (std::nothrow) T{});

        if (result)
        {
            result->construct();
        }

        return result;
    }
};


#endif
