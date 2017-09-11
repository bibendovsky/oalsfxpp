#ifndef _AL_EFFECT_H_
#define _AL_EFFECT_H_


#include <array>
#include <new>
#include <vector>
#include "alMain.h"


using EffectSampleBuffer = std::vector<float>;


union EffectProps
{
    using Pan = std::array<float, 3>;


    struct Reverb
    {
        static constexpr auto min_density = 0.0F;
        static constexpr auto max_density = 1.0F;
        static constexpr auto default_density = 1.0F;

        static constexpr auto min_diffusion = 0.0F;
        static constexpr auto max_diffusion = 1.0F;
        static constexpr auto default_diffusion = 1.0F;

        static constexpr auto min_gain = 0.0F;
        static constexpr auto max_gain = 1.0F;
        static constexpr auto default_gain = 0.32F;

        static constexpr auto min_gain_hf = 0.0F;
        static constexpr auto max_gain_hf = 1.0F;
        static constexpr auto default_gain_hf = 0.89F;

        static constexpr auto min_gain_lf = 0.0F;
        static constexpr auto max_gain_lf = 1.0F;
        static constexpr auto default_gain_lf = 1.0F;

        static constexpr auto min_decay_time = 0.1F;
        static constexpr auto max_decay_time = 20.0F;
        static constexpr auto default_decay_time = 1.49F;

        static constexpr auto min_decay_hf_ratio = 0.1F;
        static constexpr auto max_decay_hf_ratio = 2.0F;
        static constexpr auto default_decay_hf_ratio = 0.83F;

        static constexpr auto min_decay_lf_ratio = 0.1F;
        static constexpr auto max_decay_lf_ratio = 2.0F;
        static constexpr auto default_decay_lf_ratio = 1.0F;

        static constexpr auto min_reflections_gain = 0.0F;
        static constexpr auto max_reflections_gain = 3.16F;
        static constexpr auto default_reflections_gain = 0.05F;

        static constexpr auto min_reflections_delay = 0.0F;
        static constexpr auto max_reflections_delay = 0.3F;
        static constexpr auto default_reflections_delay = 0.007F;

        static constexpr auto default_reflections_pan_xyz = 0.0F;

        static constexpr auto min_late_reverb_gain = 0.0F;
        static constexpr auto max_late_reverb_gain = 10.0F;
        static constexpr auto default_late_reverb_gain = 1.26F;

        static constexpr auto min_late_reverb_delay = 0.0F;
        static constexpr auto max_late_reverb_delay = 0.1F;
        static constexpr auto default_late_reverb_delay = 0.011F;

        static constexpr auto default_late_reverb_pan_xyz = 0.0F;

        static constexpr auto min_echo_time = 0.075F;
        static constexpr auto max_echo_time = 0.25F;
        static constexpr auto default_echo_time = 0.25F;

        static constexpr auto min_echo_depth = 0.0F;
        static constexpr auto max_echo_depth = 1.0F;
        static constexpr auto default_echo_depth = 0.0F;

        static constexpr auto min_modulation_time = 0.04F;
        static constexpr auto max_modulation_time = 4.0F;
        static constexpr auto default_modulation_time = 0.25F;

        static constexpr auto min_modulation_depth = 0.0F;
        static constexpr auto max_modulation_depth = 1.0F;
        static constexpr auto default_modulation_depth = 0.0F;

        static constexpr auto min_air_absorption_gain_hf = 0.892F;
        static constexpr auto max_air_absorption_gain_hf = 1.0F;
        static constexpr auto default_air_absorption_gain_hf = 0.994F;

        static constexpr auto min_hf_reference = 1000.0F;
        static constexpr auto max_hf_reference = 20000.0F;
        static constexpr auto default_hf_reference = 5000.0F;

        static constexpr auto min_lf_reference = 20.0F;
        static constexpr auto max_lf_reference = 1000.0F;
        static constexpr auto default_lf_reference = 250.0F;

        static constexpr auto min_room_rolloff_factor = 0.0F;
        static constexpr auto max_room_rolloff_factor = 10.0F;
        static constexpr auto default_room_rolloff_factor = 0.0F;

        static constexpr auto min_decay_hf_limit = false;
        static constexpr auto max_decay_hf_limit = true;
        static constexpr auto default_decay_hf_limit = true;


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
        static constexpr auto waveform_sinusoid = 0;
        static constexpr auto waveform_triangle = 1;

        static constexpr auto min_waveform = waveform_sinusoid;
        static constexpr auto max_waveform = waveform_triangle;
        static constexpr auto default_waveform = waveform_triangle;

        static constexpr auto min_phase = -180;
        static constexpr auto max_phase = 180;
        static constexpr auto default_phase = 90;

        static constexpr auto min_rate = 0.0F;
        static constexpr auto max_rate = 10.0F;
        static constexpr auto default_rate = 1.1F;

        static constexpr auto min_depth = 0.0F;
        static constexpr auto max_depth = 1.0F;
        static constexpr auto default_depth = 0.1F;

        static constexpr auto min_feedback = -1.0F;
        static constexpr auto max_feedback = 1.0F;
        static constexpr auto default_feedback = 0.25F;

        static constexpr auto min_delay = 0.0F;
        static constexpr auto max_delay = 0.016F;
        static constexpr auto default_delay = 0.016F;


        int waveform;
        int phase;
        float rate;
        float depth;
        float feedback;
        float delay;
    }; // Chorus

    struct Compressor
    {
        static constexpr auto min_on_off = false;
        static constexpr auto max_on_off = true;
        static constexpr auto default_on_off = true;


        bool on_off;
    }; // Compressor

    struct Distortion
    {
        static constexpr auto min_edge = 0.0F;
        static constexpr auto max_edge = 1.0F;
        static constexpr auto default_edge = 0.2F;

        static constexpr auto min_gain = 0.01F;
        static constexpr auto max_gain = 1.0F;
        static constexpr auto default_gain = 0.05F;

        static constexpr auto min_low_pass_cutoff = 80.0F;
        static constexpr auto max_low_pass_cutoff = 24000.0F;
        static constexpr auto default_low_pass_cutoff = 8000.0F;

        static constexpr auto min_eq_center = 80.0F;
        static constexpr auto max_eq_center = 24000.0F;
        static constexpr auto default_eq_center = 3600.0F;

        static constexpr auto min_eq_bandwidth = 80.0F;
        static constexpr auto max_eq_bandwidth = 24000.0F;
        static constexpr auto default_eq_bandwidth = 3600.0F;


        float edge;
        float gain;
        float low_pass_cutoff;
        float eq_center;
        float eq_bandwidth;
    }; // Distortion

    struct Echo
    {
        static constexpr auto min_delay = 0.0F;
        static constexpr auto max_delay = 0.207F;
        static constexpr auto default_delay = 0.1F;

        static constexpr auto min_lr_delay = 0.0F;
        static constexpr auto max_lr_delay = 0.404F;
        static constexpr auto default_lr_delay = 0.1F;

        static constexpr auto min_damping = 0.0F;
        static constexpr auto max_damping = 0.99F;
        static constexpr auto default_damping = 0.5F;

        static constexpr auto min_feedback = 0.0F;
        static constexpr auto max_feedback = 1.0F;
        static constexpr auto default_feedback = 0.5F;

        static constexpr auto min_spread = -1.0F;
        static constexpr auto max_spread = 1.0F;
        static constexpr auto default_spread = -1.0F;


        float delay;
        float lr_delay;

        float damping;
        float feedback;

        float spread;
    }; // Echo

    struct Equalizer
    {
        static constexpr auto min_low_gain = 0.126F;
        static constexpr auto max_low_gain = 7.943F;
        static constexpr auto default_low_gain = 1.0F;

        static constexpr auto min_low_cutoff = 50.0F;
        static constexpr auto max_low_cutoff = 800.0F;
        static constexpr auto default_low_cutoff = 200.0F;

        static constexpr auto min_mid1_gain = 0.126F;
        static constexpr auto max_mid1_gain = 7.943F;
        static constexpr auto default_mid1_gain = 1.0F;

        static constexpr auto min_mid1_center = 200.0F;
        static constexpr auto max_mid1_center = 3000.0F;
        static constexpr auto default_mid1_center = 500.0F;

        static constexpr auto min_mid1_width = 0.01F;
        static constexpr auto max_mid1_width = 1.0F;
        static constexpr auto default_mid1_width = 1.0F;

        static constexpr auto min_mid2_gain = 0.126F;
        static constexpr auto max_mid2_gain = 7.943F;
        static constexpr auto default_mid2_gain = 1.0F;

        static constexpr auto min_mid2_center = 1000.0F;
        static constexpr auto max_mid2_center = 8000.0F;
        static constexpr auto default_mid2_center = 3000.0F;

        static constexpr auto min_mid2_width = 0.01F;
        static constexpr auto max_mid2_width = 1.0F;
        static constexpr auto default_mid2_width = 1.0F;

        static constexpr auto min_high_gain = 0.126F;
        static constexpr auto max_high_gain = 7.943F;
        static constexpr auto default_high_gain = 1.0F;

        static constexpr auto min_high_cutoff = 4000.0F;
        static constexpr auto max_high_cutoff = 16000.0F;
        static constexpr auto default_high_cutoff = 6000.0F;


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
        static constexpr auto waveform_sinusoid = 0;
        static constexpr auto waveform_triangle = 1;

        static constexpr auto min_waveform = waveform_sinusoid;
        static constexpr auto max_waveform = waveform_triangle;
        static constexpr auto default_waveform = waveform_triangle;

        static constexpr auto min_phase = -180;
        static constexpr auto max_phase = 180;
        static constexpr auto default_phase = 0;

        static constexpr auto min_rate = 0.0F;
        static constexpr auto max_rate = 10.0F;
        static constexpr auto default_rate = 0.27F;

        static constexpr auto min_depth = 0.0F;
        static constexpr auto max_depth = 1.0F;
        static constexpr auto default_depth = 1.0F;

        static constexpr auto min_feedback = -1.0F;
        static constexpr auto max_feedback = 1.0F;
        static constexpr auto default_feedback = -0.5F;

        static constexpr auto min_delay = 0.0F;
        static constexpr auto max_delay = 0.004F;
        static constexpr auto default_delay = 0.002F;


        int waveform;
        int phase;
        float rate;
        float depth;
        float feedback;
        float delay;
    }; // Flanger

    struct Modulator
    {
        static constexpr auto min_frequency = 0.0F;
        static constexpr auto max_frequency = 8000.0F;
        static constexpr auto default_frequency = 440.0F;

        static constexpr auto min_high_pass_cutoff = 0.0F;
        static constexpr auto max_high_pass_cutoff = 24000.0F;
        static constexpr auto default_high_pass_cutoff = 800.0F;

        static constexpr auto waveform_sinusoid = 0;
        static constexpr auto waveform_sawtooth = 1;
        static constexpr auto waveform_square = 2;

        static constexpr auto min_waveform = waveform_sinusoid;
        static constexpr auto max_waveform = waveform_square;
        static constexpr auto default_waveform = waveform_sinusoid;


        float frequency;
        float high_pass_cutoff;
        int waveform;
    }; // Modulator

    struct Dedicated
    {
        static constexpr auto min_gain = 0.0F;
        static constexpr auto max_gain = 1.0F;
        static constexpr auto default_gain = 1.0F;


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
    EffectType type_;

    EffectProps props_;


    Effect();

    void initialize(
        const EffectType type = EffectType::null);
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


struct EffectStateDeleter
{
    void operator()(
        EffectState* effect_state);
};


class EffectStateFactory
{
public:
    static EffectState* create_by_type(
        const EffectType type);


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
