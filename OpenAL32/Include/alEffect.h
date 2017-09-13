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
        float density_;
        float diffusion_;
        float gain_;
        float gain_hf_;
        float decay_time_;
        float decay_hf_ratio_;
        float reflections_gain_;
        float reflections_delay_;
        float late_reverb_gain_;
        float late_reverb_delay_;
        float air_absorption_gain_hf_;
        float room_rolloff_factor_;
        bool decay_hf_limit_;

        // Additional EAX reverb properties
        float gain_lf_;
        float decay_lf_ratio_;
        Pan reflections_pan_;
        Pan late_reverb_pan_;
        float echo_time_;
        float echo_depth_;
        float modulation_time_;
        float modulation_depth_;
        float hf_reference_;
        float lf_reference_;
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


        int waveform_;
        int phase_;
        float rate_;
        float depth_;
        float feedback_;
        float delay_;
    }; // Chorus

    struct Compressor
    {
        static constexpr auto min_on_off = false;
        static constexpr auto max_on_off = true;
        static constexpr auto default_on_off = true;


        bool on_off_;
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


        float edge_;
        float gain_;
        float low_pass_cutoff_;
        float eq_center_;
        float eq_bandwidth_;
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


        float delay_;
        float lr_delay_;

        float damping_;
        float feedback_;

        float spread_;
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


        float low_cutoff_;
        float low_gain_;
        float mid1_center_;
        float mid1_gain_;
        float mid1_width_;
        float mid2_center_;
        float mid2_gain_;
        float mid2_width_;
        float high_cutoff_;
        float high_gain_;
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


        int waveform_;
        int phase_;
        float rate_;
        float depth_;
        float feedback_;
        float delay_;
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


        float frequency_;
        float high_pass_cutoff_;
        int waveform_;
    }; // Modulator

    struct Dedicated
    {
        static constexpr auto min_gain = 0.0F;
        static constexpr auto max_gain = 1.0F;
        static constexpr auto default_gain = 1.0F;


        float gain_;
    }; // Dedicated


    Reverb reverb_;
    Chorus chorus_;
    Compressor compressor_;
    Distortion distortion_;
    Echo echo_;
    Equalizer equalizer_;
    Flanger flanger_;
    Modulator modulator_;
    Dedicated dedicated_;
}; // EffectProps


struct Effect
{
    EffectType type_;
    EffectProps props_;


    Effect()
        :
        type_{},
        props_{}
    {
    }

    void initialize(
        const EffectType type = EffectType::null)
    {
        switch (type)
        {
        case EffectType::reverb:
        case EffectType::eax_reverb:
            props_.reverb_.density_ = EffectProps::Reverb::default_density;
            props_.reverb_.diffusion_ = EffectProps::Reverb::default_diffusion;
            props_.reverb_.gain_ = EffectProps::Reverb::default_gain;
            props_.reverb_.gain_hf_ = EffectProps::Reverb::default_gain_hf;
            props_.reverb_.gain_lf_ = EffectProps::Reverb::default_gain_lf;
            props_.reverb_.decay_time_ = EffectProps::Reverb::default_decay_time;
            props_.reverb_.decay_hf_ratio_ = EffectProps::Reverb::default_decay_hf_ratio;
            props_.reverb_.decay_lf_ratio_ = EffectProps::Reverb::default_decay_lf_ratio;
            props_.reverb_.reflections_gain_ = EffectProps::Reverb::default_reflections_gain;
            props_.reverb_.reflections_delay_ = EffectProps::Reverb::default_reflections_delay;
            props_.reverb_.reflections_pan_[0] = EffectProps::Reverb::default_reflections_pan_xyz;
            props_.reverb_.reflections_pan_[1] = EffectProps::Reverb::default_reflections_pan_xyz;
            props_.reverb_.reflections_pan_[2] = EffectProps::Reverb::default_reflections_pan_xyz;
            props_.reverb_.late_reverb_gain_ = EffectProps::Reverb::default_late_reverb_gain;
            props_.reverb_.late_reverb_delay_ = EffectProps::Reverb::default_late_reverb_delay;
            props_.reverb_.late_reverb_pan_[0] = EffectProps::Reverb::default_late_reverb_pan_xyz;
            props_.reverb_.late_reverb_pan_[1] = EffectProps::Reverb::default_late_reverb_pan_xyz;
            props_.reverb_.late_reverb_pan_[2] = EffectProps::Reverb::default_late_reverb_pan_xyz;
            props_.reverb_.echo_time_ = EffectProps::Reverb::default_echo_time;
            props_.reverb_.echo_depth_ = EffectProps::Reverb::default_echo_depth;
            props_.reverb_.modulation_time_ = EffectProps::Reverb::default_modulation_time;
            props_.reverb_.modulation_depth_ = EffectProps::Reverb::default_modulation_depth;
            props_.reverb_.air_absorption_gain_hf_ = EffectProps::Reverb::default_air_absorption_gain_hf;
            props_.reverb_.hf_reference_ = EffectProps::Reverb::default_hf_reference;
            props_.reverb_.lf_reference_ = EffectProps::Reverb::default_lf_reference;
            props_.reverb_.room_rolloff_factor_ = EffectProps::Reverb::default_room_rolloff_factor;
            props_.reverb_.decay_hf_limit_ = EffectProps::Reverb::default_decay_hf_limit;
            break;

        case EffectType::chorus:
            props_.chorus_.waveform_ = EffectProps::Chorus::default_waveform;
            props_.chorus_.phase_ = EffectProps::Chorus::default_phase;
            props_.chorus_.rate_ = EffectProps::Chorus::default_rate;
            props_.chorus_.depth_ = EffectProps::Chorus::default_depth;
            props_.chorus_.feedback_ = EffectProps::Chorus::default_feedback;
            props_.chorus_.delay_ = EffectProps::Chorus::default_delay;
            break;

        case EffectType::compressor:
            props_.compressor_.on_off_ = EffectProps::Compressor::default_on_off;
            break;

        case EffectType::distortion:
            props_.distortion_.edge_ = EffectProps::Distortion::default_edge;
            props_.distortion_.gain_ = EffectProps::Distortion::default_gain;
            props_.distortion_.low_pass_cutoff_ = EffectProps::Distortion::default_low_pass_cutoff;
            props_.distortion_.eq_center_ = EffectProps::Distortion::default_eq_center;
            props_.distortion_.eq_bandwidth_ = EffectProps::Distortion::default_eq_bandwidth;
            break;

        case EffectType::echo:
            props_.echo_.delay_ = EffectProps::Echo::default_delay;
            props_.echo_.lr_delay_ = EffectProps::Echo::default_lr_delay;
            props_.echo_.damping_ = EffectProps::Echo::default_damping;
            props_.echo_.feedback_ = EffectProps::Echo::default_feedback;
            props_.echo_.spread_ = EffectProps::Echo::default_spread;
            break;

        case EffectType::equalizer:
            props_.equalizer_.low_cutoff_ = EffectProps::Equalizer::default_low_cutoff;
            props_.equalizer_.low_gain_ = EffectProps::Equalizer::default_high_gain;
            props_.equalizer_.mid1_center_ = EffectProps::Equalizer::default_mid1_center;
            props_.equalizer_.mid1_gain_ = EffectProps::Equalizer::default_mid1_gain;
            props_.equalizer_.mid1_width_ = EffectProps::Equalizer::default_mid1_width;
            props_.equalizer_.mid2_center_ = EffectProps::Equalizer::default_mid2_center;
            props_.equalizer_.mid2_gain_ = EffectProps::Equalizer::default_mid2_gain;
            props_.equalizer_.mid2_width_ = EffectProps::Equalizer::default_mid2_width;
            props_.equalizer_.high_cutoff_ = EffectProps::Equalizer::default_high_cutoff;
            props_.equalizer_.high_gain_ = EffectProps::Equalizer::default_high_gain;
            break;

        case EffectType::flanger:
            props_.flanger_.waveform_ = EffectProps::Flanger::default_waveform;
            props_.flanger_.phase_ = EffectProps::Flanger::default_phase;
            props_.flanger_.rate_ = EffectProps::Flanger::default_rate;
            props_.flanger_.depth_ = EffectProps::Flanger::default_depth;
            props_.flanger_.feedback_ = EffectProps::Flanger::default_feedback;
            props_.flanger_.delay_ = EffectProps::Flanger::default_delay;
            break;

        case EffectType::ring_modulator:
            props_.modulator_.frequency_ = EffectProps::Modulator::default_frequency;
            props_.modulator_.high_pass_cutoff_ = EffectProps::Modulator::default_high_pass_cutoff;
            props_.modulator_.waveform_ = EffectProps::Modulator::default_waveform;
            break;

        case EffectType::dedicated_dialog:
        case EffectType::dedicated_low_frequency:
            props_.dedicated_.gain_ = EffectProps::Dedicated::default_gain;
            break;

        default:
            break;
        }

        type_ = type;
    }

}; // Effect


class EffectState
{
public:
    EffectState(
        const EffectState& that) = delete;

    EffectState& operator=(
        const EffectState& that) = delete;

    virtual ~EffectState()
    {
    }


    SampleBuffers* dst_buffers_;
    int dst_channel_count_;


    void construct()
    {
        do_construct();
    }

    void destruct()
    {
        do_destruct();
    }

    void update_device(
        ALCdevice* device)
    {
        do_update_device(device);
    }

    void update(
        ALCdevice* device,
        const EffectSlot* slot,
        const EffectProps *props)
    {
        do_update(device, slot, props);
    }

    void process(
        int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count)
    {
        do_process(sample_count, src_samples, dst_samples, channel_count);
    }

    static void destroy(
        EffectState*& effect_state)
    {
        if (!effect_state)
        {
            return;
        }

        effect_state->destruct();
        delete effect_state;
        effect_state = nullptr;
    }


protected:
    EffectState()
        :
        dst_buffers_{},
        dst_channel_count_{}
    {
    }


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
        EffectState* effect_state)
    {
        if (!effect_state)
        {
            return;
        }

        effect_state->destruct();
        delete effect_state;
    }
};


class EffectStateFactory
{
public:
    static EffectState* create_by_type(
        const EffectType type)
    {
        switch (type)
        {
        case EffectType::null:
            return create_null();

        case EffectType::eax_reverb:
            return create_reverb();

        case EffectType::reverb:
            return create_reverb();

        case EffectType::chorus:
            return create_chorus();

        case EffectType::compressor:
            return create_compressor();

        case EffectType::distortion:
            return create_distortion();

        case EffectType::echo:
            return create_echo();

        case EffectType::equalizer:
            return create_equalizer();

        case EffectType::flanger:
            return create_flanger();

        case EffectType::ring_modulator:
            return create_modulator();

        case EffectType::dedicated_dialog:
        case EffectType::dedicated_low_frequency:
            return create_dedicated();

        default:
            return nullptr;
        }
    }


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
