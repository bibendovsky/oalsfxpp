/**
 * Ambisonic reverb engine for the OpenAL cross platform audio library
 * Copyright (C) 2008-2017 by Chris Robinson and Christopher Fitzgerald.
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */


#include <cstdint>
#include <algorithm>
#include "config.h"
#include "alu.h"
#include "mixer_defs.h"


class ReverbEffect :
    public IEffect
{
public:
    ReverbEffect()
        :
        IEffect{},
        is_eax_{},
        sample_buffer_{},
        total_samples_{},
        filters_{},
        delay_{},
        early_delay_taps_{},
        early_delay_coeffs_{},
        late_feed_tap_{},
        late_delay_taps_{},
        ap_feed_coeff_{},
        mix_x_{},
        mix_y_{},
        early_{},
        mod_{},
        late_{},
        fade_count_{},
        offset_{},
        a_format_samples_{},
        reverb_samples_{},
        early_samples_{}
    {
    }

    virtual ~ReverbEffect()
    {
    }


protected:
    void ReverbEffect::do_construct() final
    {
        is_eax_ = AL_FALSE;

        total_samples_ = 0;
        sample_buffer_ = ReverbSampleBuffer{};

        for (int i = 0; i < 4; ++i)
        {
            ALfilterState_clear(&filters_[i].lp);
            ALfilterState_clear(&filters_[i].hp);
        }

        delay_.mask = 0;
        delay_.lines = nullptr;

        for (int i = 0; i < 4; ++i)
        {
            early_delay_taps_[i][0] = 0;
            early_delay_taps_[i][1] = 0;
            early_delay_coeffs_[i] = 0.0f;
        }

        late_feed_tap_ = 0;

        for (int i = 0; i < 4; ++i)
        {
            late_delay_taps_[i][0] = 0;
            late_delay_taps_[i][1] = 0;
        }

        ap_feed_coeff_ = 0.0f;
        mix_x_ = 0.0f;
        mix_y_ = 0.0f;

        early_.vec_ap.delay.mask = 0;
        early_.vec_ap.delay.lines = nullptr;
        early_.delay.mask = 0;
        early_.delay.lines = nullptr;

        for (int i = 0; i < 4; ++i)
        {
            early_.vec_ap.offsets[i][0] = 0;
            early_.vec_ap.offsets[i][1] = 0;
            early_.offsets[i][0] = 0;
            early_.offsets[i][1] = 0;
            early_.coeffs[i] = 0.0F;
        }

        mod_.index = 0;
        mod_.range = 1;
        mod_.depth = 0.0F;
        mod_.coeff = 0.0F;
        mod_.filter = 0.0F;

        late_.density_gain = 0.0F;

        late_.delay.mask = 0;
        late_.delay.lines = nullptr;
        late_.vec_ap.delay.mask = 0;
        late_.vec_ap.delay.lines = nullptr;

        for (int i = 0; i < 4; ++i)
        {
            late_.offsets[i][0] = 0;
            late_.offsets[i][1] = 0;

            late_.vec_ap.offsets[i][0] = 0;
            late_.vec_ap.offsets[i][1] = 0;

            for (int j = 0; j < 3; ++j)
            {
                late_.filters[i].lf_coeffs[j] = 0.0F;
                late_.filters[i].hf_coeffs[j] = 0.0F;
            }

            late_.filters[i].mid_coeff = 0.0F;

            late_.filters[i].states[0][0] = 0.0F;
            late_.filters[i].states[0][1] = 0.0F;
            late_.filters[i].states[1][0] = 0.0F;
            late_.filters[i].states[1][1] = 0.0F;
        }

        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < MAX_OUTPUT_CHANNELS; ++j)
            {
                early_.current_gains[i][j] = 0.0F;
                early_.pan_gains[i][j] = 0.0F;
                late_.current_gains[i][j] = 0.0F;
                late_.pan_gains[i][j] = 0.0F;
            }
        }

        fade_count_ = 0;
        offset_ = 0;
    }

    void ReverbEffect::do_destruct() final
    {
        sample_buffer_ = ReverbSampleBuffer{};
    }

    ALboolean ReverbEffect::do_update_device(
        ALCdevice* device) final
    {
        const auto frequency = device->frequency;

        // Allocate the delay lines.
        if (!alloc_lines(frequency))
        {
            return AL_FALSE;
        }

        // Calculate the modulation filter coefficient.  Notice that the exponent
        // is calculated given the current sample rate.  This ensures that the
        // resulting filter response over time is consistent across all sample
        // rates.
        mod_.coeff = std::pow(modulation_filter_coeff, modulation_filter_const / frequency);

        const auto multiplier = 1.0F + line_multiplier;

        // The late feed taps are set a fixed position past the latest delay tap.
        for (int i = 0; i < 4; ++i)
        {
            late_feed_tap_ = fastf2i(
                (AL_EAXREVERB_MAX_REFLECTIONS_DELAY + (early_tap_lengths[3] * multiplier)) * frequency);
        }

        return AL_TRUE;
    }

    void ReverbEffect::do_update(
        ALCdevice* device,
        const struct ALeffectslot* slot,
        const union ALeffectProps* props) final
    {
        if (slot->params.effect_type == AL_EFFECT_EAXREVERB)
        {
            is_eax_ = AL_TRUE;
        }
        else if (slot->params.effect_type == AL_EFFECT_REVERB)
        {
            is_eax_ = AL_FALSE;
        }

        const auto frequency = device->frequency;

        // Calculate the master filters
        const auto hf_scale = props->reverb.hf_reference / frequency;

        // Restrict the filter gains from going below -60dB to keep the filter from
        // killing most of the signal.
        const auto gain_hf = std::max(props->reverb.gain_hf, 0.001F);

        ALfilterState_setParams(
            &filters_[0].lp,
            ALfilterType_HighShelf,
            gain_hf,
            hf_scale,
            calc_rcpQ_from_slope(gain_hf, 1.0F));

        const auto lf_scale = props->reverb.lf_reference / frequency;

        const auto gain_lf = std::max(props->reverb.gain_lf, 0.001F);

        ALfilterState_setParams(
            &filters_[0].hp,
            ALfilterType_LowShelf,
            gain_lf,
            lf_scale,
            calc_rcpQ_from_slope(gain_lf, 1.0F));

        for (int i = 1; i < 4; ++i)
        {
            ALfilterState_copyParams(&filters_[i].lp, &filters_[0].lp);
            ALfilterState_copyParams(&filters_[i].hp, &filters_[0].hp);
        }

        // Update the main effect delay and associated taps.
        update_delay_line(
            props->reverb.reflections_delay,
            props->reverb.late_reverb_delay,
            props->reverb.density,
            props->reverb.decay_time,
            frequency);

        // Calculate the all-pass feed-back/forward coefficient.
        ap_feed_coeff_ = std::sqrt(0.5F) * std::pow(props->reverb.diffusion, 2.0F);

        // Update the early lines.
        update_early_lines(props->reverb.density, props->reverb.decay_time, frequency);

        // Get the mixing matrix coefficients.
        calc_matrix_coeffs(props->reverb.diffusion, &mix_x_, &mix_y_);

        // If the HF limit parameter is flagged, calculate an appropriate limit
        // based on the air absorption parameter.
        auto hf_ratio = props->reverb.decay_hf_ratio;

        if (props->reverb.decay_hf_limit && props->reverb.air_absorption_gain_hf < 1.0F)
        {
            hf_ratio = calc_limited_hf_ratio(
                hf_ratio,
                props->reverb.air_absorption_gain_hf,
                props->reverb.decay_time);
        }

        // Calculate the LF/HF decay times.
        const auto lf_decay_time = clampf(
            props->reverb.decay_time * props->reverb.decay_lf_ratio,
            AL_EAXREVERB_MIN_DECAY_TIME,
            AL_EAXREVERB_MAX_DECAY_TIME);

        const auto hf_decay_time = clampf(
            props->reverb.decay_time * hf_ratio,
            AL_EAXREVERB_MIN_DECAY_TIME,
            AL_EAXREVERB_MAX_DECAY_TIME);

        // Update the modulator line.
        update_modulator(props->reverb.modulation_time, props->reverb.modulation_depth, frequency);

        // Update the late lines.
        update_late_lines(
            props->reverb.density,
            props->reverb.diffusion,
            lf_decay_time,
            props->reverb.decay_time,
            hf_decay_time,
            F_TAU * lf_scale,
            F_TAU * hf_scale,
            props->reverb.echo_time,
            props->reverb.echo_depth,
            frequency);

        // Update early and late 3D panning.
        update_3d_panning(
            device,
            props->reverb.reflections_pan,
            props->reverb.late_reverb_pan,
            props->reverb.gain,
            props->reverb.reflections_gain,
            props->reverb.late_reverb_gain);

        // Determine if delay-line cross-fading is required.
        for (int i = 0; i < 4; ++i)
        {
            if (early_delay_taps_[i][1] != early_delay_taps_[i][0] ||
                early_.vec_ap.offsets[i][1] != early_.vec_ap.offsets[i][0] ||
                early_.offsets[i][1] != early_.offsets[i][0] ||
                late_delay_taps_[i][1] != late_delay_taps_[i][0] ||
                late_.vec_ap.offsets[i][1] != late_.vec_ap.offsets[i][0] ||
                late_.offsets[i][1] != late_.offsets[i][0])
            {
                fade_count_ = 0;
                break;
            }
        }
    }

    void ReverbEffect::do_process(
        const ALsizei sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const ALsizei channel_count) final
    {
        const auto reverb_func = (is_eax_ ? &ReverbEffect::eax_verb_pass : &ReverbEffect::verb_pass);
        auto fade = static_cast<ALfloat>(fade_count_) / fade_samples;

        // Process reverb for these samples.
        for (int base = 0; base < sample_count; )
        {
            auto todo = std::min(sample_count - base, max_update_samples);

            // If cross-fading, don't do more samples than there are to fade.
            if (fade_samples - fade_count_ > 0)
            {
                todo = std::min(todo, fade_samples - fade_count_);
            }

            // Convert B-Format to A-Format for processing.
            memset(a_format_samples_, 0, sizeof(*a_format_samples_) * 4);

            for (int c = 0; c < 4; ++c)
            {
                MixRow_C(
                    a_format_samples_[c],
                    b2a.m[c],
                    src_samples,
                    MAX_EFFECT_CHANNELS,
                    base,
                    todo);
            }

            // Process the samples for reverb.
            fade = (this->*reverb_func)(todo, fade, a_format_samples_, early_samples_, reverb_samples_);

            if (fade_count_ < fade_samples)
            {
                fade_count_ += todo;

                if (fade_count_ >= fade_samples)
                {
                    // Update the cross-fading delay line taps.
                    fade_count_ = fade_samples;
                    fade = 1.0F;

                    for (int c = 0; c < 4; ++c)
                    {
                        early_delay_taps_[c][0] = early_delay_taps_[c][1];
                        early_.vec_ap.offsets[c][0] = early_.vec_ap.offsets[c][1];
                        early_.offsets[c][0] = early_.offsets[c][1];
                        late_delay_taps_[c][0] = late_delay_taps_[c][1];
                        late_.vec_ap.offsets[c][0] = late_.vec_ap.offsets[c][1];
                        late_.offsets[c][0] = late_.offsets[c][1];
                    }
                }
            }

            // Mix the A-Format results to output, implicitly converting back to
            // B-Format.
            for (int c = 0; c < 4; c++)
            {
                Mix_C(
                    early_samples_[c],
                    channel_count,
                    dst_samples,
                    early_.current_gains[c],
                    early_.pan_gains[c],
                    sample_count - base,
                    base,
                    todo);
            }

            for (int c = 0; c < 4; c++)
            {
                Mix_C(
                    reverb_samples_[c],
                    channel_count,
                    dst_samples,
                    late_.current_gains[c],
                    late_.pan_gains[c],
                    sample_count - base,
                    base,
                    todo);
            }

            base += todo;
        }
    }


private:
    // This is the maximum number of samples processed for each inner loop
    // iteration.
    static constexpr auto max_update_samples = 256;

    // The number of samples used for cross-faded delay lines.  This can be used
    // to balance the compensation for abrupt line changes and attenuation due to
    // minimally lengthed recursive lines.  Try to keep this below the device
    // update size.
    static constexpr auto fade_samples = 128;


    struct DelayLineI
    {
        // The delay lines use interleaved samples, with the lengths being powers
        // of 2 to allow the use of bit-masking instead of a modulus for wrapping.

        ALsizei mask;
        ALfloat (*lines)[4];
    }; // DelayLineI

    struct VecAllpass
    {
        using Offsets = ALsizei[4][2];

        DelayLineI delay;
        Offsets offsets;
    }; // VecAllpass

    using ReverbSampleBuffer = EffectSampleBuffer;


    struct Filter
    {
        ALfilterState lp;
        ALfilterState hp; // EAX only
    }; // Filter

    using Filters = Filter[4];

    struct Early
    {
        using Offsets = ALsizei[4][2];
        using Coeffs = ALfloat[4];
        using Gains = ALfloat[4][MAX_OUTPUT_CHANNELS];

        // A Gerzon vector all-pass filter is used to simulate initial
        // diffusion.  The spread from this filter also helps smooth out the
        // reverb tail.
        //
        VecAllpass vec_ap;

        // An echo line is used to complete the second half of the early
        // reflections.
        //
        DelayLineI delay;
        Offsets offsets;
        Coeffs coeffs;

        // The gain for each output channel based on 3D panning.
        Gains current_gains;
        Gains pan_gains;
    }; // Early

    struct Mod
    {
        // The vibrato time is tracked with an index over a modulus-wrapped
        // range (in samples).
        //
        ALuint index;
        ALuint range;

        // The depth of frequency change (also in samples) and its filter.
        ALfloat depth;
        ALfloat coeff;
        ALfloat filter;
    }; // Mod

    struct Late
    {
        struct Filter
        {
            using Coeffs = ALfloat[3];
            using States = ALfloat[2][2];

            Coeffs lf_coeffs;
            Coeffs hf_coeffs;
            ALfloat mid_coeff;

            // The LF and HF filters keep a state of the last input and last
            // output sample.
            States states;
        }; // Filter

        using Filters = Filter[4];
        using Offsets = ALsizei[4][2];
        using Gains = ALfloat[4][MAX_OUTPUT_CHANNELS];


        // Attenuation to compensate for the modal density and decay rate of
        // the late lines.
        //
        ALfloat density_gain;

        // A recursive delay line is used fill in the reverb tail.
        DelayLineI delay;
        Offsets offsets;

        // T60 decay filters are used to simulate absorption.
        Filters filters;

        // A Gerzon vector all-pass filter is used to simulate diffusion.
        VecAllpass vec_ap;

        // The gain for each output channel based on 3D panning.
        Gains current_gains;
        Gains pan_gains;
    }; // Late

    using Taps = ALsizei[4][2];
    using Samples = ALfloat[4][max_update_samples];


    ALboolean is_eax_;

    // All delay lines are allocated as a single buffer to reduce memory
    // fragmentation and management code.
    //
    ReverbSampleBuffer sample_buffer_;
    ALuint total_samples_;

    // Master effect filters
    Filters filters_;

    // Core delay line (early reflections and late reverb tap from this).
    DelayLineI delay_;

    // Tap points for early reflection delay.
    Taps early_delay_taps_;
    ALfloat early_delay_coeffs_[4];

    // Tap points for late reverb feed and delay.
    ALsizei late_feed_tap_;
    Taps late_delay_taps_;

    // The feed-back and feed-forward all-pass coefficient.
    ALfloat ap_feed_coeff_;

    // Coefficients for the all-pass and line scattering matrices.
    ALfloat mix_x_;
    ALfloat mix_y_;

    Early early_;
    Mod mod_; // EAX only
    Late late_;

    // Indicates the cross-fade point for delay line reads [0,FADE_SAMPLES].
    ALsizei fade_count_;

    // The current write offset for all delay lines.
    ALsizei offset_;

    // Temporary storage used when processing.
    Samples a_format_samples_;
    Samples reverb_samples_;
    Samples early_samples_;


    // The B-Format to A-Format conversion matrix. The arrangement of rows is
    // deliberately chosen to align the resulting lines to their spatial opposites
    // (0:above front left <-> 3:above back right, 1:below front right <-> 2:below
    // back left). It's not quite opposite, since the A-Format results in a
    // tetrahedron, but it's close enough. Should the model be extended to 8-lines
    // in the future, true opposites can be used.
    static constexpr aluMatrixf b2a = {{
        { 0.288675134595F,  0.288675134595F,  0.288675134595F,  0.288675134595F },
        { 0.288675134595F, -0.288675134595F, -0.288675134595F,  0.288675134595F },
        { 0.288675134595F,  0.288675134595F, -0.288675134595F, -0.288675134595F },
        { 0.288675134595F, -0.288675134595F,  0.288675134595F, -0.288675134595F },
    }};

    // Converts A-Format to B-Format.
    static constexpr aluMatrixf a2b = {{
        { 0.866025403785F,  0.866025403785F,  0.866025403785F,  0.866025403785F },
        { 0.866025403785F, -0.866025403785F,  0.866025403785F, -0.866025403785F },
        { 0.866025403785F, -0.866025403785F, -0.866025403785F,  0.866025403785F },
        { 0.866025403785F,  0.866025403785F, -0.866025403785F, -0.866025403785F },
    }};

    static constexpr auto fade_step = 1.0F / fade_samples;

    // The all-pass and delay lines have a variable length dependent on the
    // effect's density parameter.  The resulting density multiplier is:
    //
    //     multiplier = 1 + (density * LINE_MULTIPLIER)
    //
    // Thus the line multiplier below will result in a maximum density multiplier
    // of 10.
    static constexpr auto line_multiplier = 9.0F;

    // All delay line lengths are specified in seconds.
    //
    // To approximate early reflections, we break them up into primary (those
    // arriving from the same direction as the source) and secondary (those
    // arriving from the opposite direction).
    //
    // The early taps decorrelate the 4-channel signal to approximate an average
    // room response for the primary reflections after the initial early delay.
    //
    // Given an average room dimension (d_a) and the speed of sound (c) we can
    // calculate the average reflection delay (r_a) regardless of listener and
    // source positions as:
    //
    //     r_a = d_a / c
    //     c   = 343.3
    //
    // This can extended to finding the average difference (r_d) between the
    // maximum (r_1) and minimum (r_0) reflection delays:
    //
    //     r_0 = 2 / 3 r_a
    //         = r_a - r_d / 2
    //         = r_d
    //     r_1 = 4 / 3 r_a
    //         = r_a + r_d / 2
    //         = 2 r_d
    //     r_d = 2 / 3 r_a
    //         = r_1 - r_0
    //
    // As can be determined by integrating the 1D model with a source (s) and
    // listener (l) positioned across the dimension of length (d_a):
    //
    //     r_d = int_(l=0)^d_a (int_(s=0)^d_a |2 d_a - 2 (l + s)| ds) dl / c
    //
    // The initial taps (T_(i=0)^N) are then specified by taking a power series
    // that ranges between r_0 and half of r_1 less r_0:
    //
    //     R_i = 2^(i / (2 N - 1)) r_d
    //         = r_0 + (2^(i / (2 N - 1)) - 1) r_d
    //         = r_0 + T_i
    //     T_i = R_i - r_0
    //         = (2^(i / (2 N - 1)) - 1) r_d
    //
    // Assuming an average of 5m (up to 50m with the density multiplier), we get
    // the following taps:
    static constexpr ALfloat early_tap_lengths[4] =
    {
        0.000000e+0F, 1.010676E-3F, 2.126553E-3F, 3.358580E-3F,
    };

    // The early all-pass filter lengths are based on the early tap lengths:
    //
    //     A_i = R_i / a
    //
    // Where a is the approximate maximum all-pass cycle limit (20).
    //
    static constexpr ALfloat early_allpass_lengths[4] =
    {
        4.854840E-4F, 5.360178E-4F, 5.918117E-4F, 6.534130E-4F,
    };

    // The early delay lines are used to transform the primary reflections into
    // the secondary reflections.  The A-format is arranged in such a way that
    // the channels/lines are spatially opposite:
    //
    //     C_i is opposite C_(N-i-1)
    //
    // The delays of the two opposing reflections (R_i and O_i) from a source
    // anywhere along a particular dimension always sum to twice its full delay:
    //
    //     2 r_a = R_i + O_i
    //
    // With that in mind we can determine the delay between the two reflections
    // and thus specify our early line lengths (L_(i=0)^N) using:
    //
    //     O_i = 2 r_a - R_(N-i-1)
    //     L_i = O_i - R_(N-i-1)
    //         = 2 (r_a - R_(N-i-1))
    //         = 2 (r_a - T_(N-i-1) - r_0)
    //         = 2 r_a (1 - (2 / 3) 2^((N - i - 1) / (2 N - 1)))
    //
    // Using an average dimension of 5m, we get:
    static constexpr ALfloat early_line_lengths[4] =
    {
        2.992520E-3F, 5.456575E-3F, 7.688329E-3F, 9.709681E-3F,
    };

    // The late all-pass filter lengths are based on the late line lengths:
    //
    //     A_i = (5 / 3) L_i / r_1
    //
    static constexpr ALfloat late_allpass_lengths[4] =
    {
        8.091400E-4F, 1.019453E-3F, 1.407968E-3F, 1.618280E-3F,
    };

    // The late lines are used to approximate the decaying cycle of recursive
    // late reflections.
    //
    // Splitting the lines in half, we start with the shortest reflection paths
    // (L_(i=0)^(N/2)):
    //
    //     L_i = 2^(i / (N - 1)) r_d
    //
    // Then for the opposite (longest) reflection paths (L_(i=N/2)^N):
    //
    //     L_i = 2 r_a - L_(i-N/2)
    //         = 2 r_a - 2^((i - N / 2) / (N - 1)) r_d
    //
    // For our 5m average room, we get:
    static constexpr ALfloat late_line_lengths[4] =
    {
        9.709681E-3F, 1.223343E-2F, 1.689561E-2F, 1.941936E-2F,
    };

    // This coefficient is used to define the sinus depth according to the
    // modulation depth property. This value must be below half the shortest late
    // line length (0.0097/2 = ~0.0048), otherwise with certain parameters (high
    // mod time, low density) the downswing can sample before the input.
    static constexpr ALfloat modulation_depth_coeff = 1.0f / 4096.0f;

    // A filter is used to avoid the terrible distortion caused by changing
    // modulation time and/or depth.  To be consistent across different sample
    // rates, the coefficient must be raised to a constant divided by the sample
    // rate:  coeff^(constant / rate).
    static constexpr ALfloat modulation_filter_coeff = 0.048F;
    static constexpr ALfloat modulation_filter_const = 100000.0F;


    //
    // Device Update
    //

    // Given the allocated sample buffer, this function updates each delay line
    // offset.
    static void realize_line_offset(
        ReverbSampleBuffer& sample_buffer,
        DelayLineI* delay)
    {
        auto ptr1 = &sample_buffer[reinterpret_cast<intptr_t>(delay->lines) * 4];
        auto ptr2 = reinterpret_cast<ALfloat (*)[4]>(ptr1);

        delay->lines = ptr2;
    }

    // Calculate the length of a delay line and store its mask and offset.
    static ALuint calc_line_length(
        const ALfloat length,
        const ptrdiff_t offset,
        const ALuint frequency,
        const ALuint extra,
        DelayLineI *delay)
    {
        auto samples = ALuint{};

        // All line lengths are powers of 2, calculated from their lengths in
        // seconds, rounded up.
        samples = fastf2i(std::ceil(length * frequency));
        samples = NextPowerOf2(samples + extra);

        // All lines share a single sample buffer.
        delay->mask = samples - 1;
        delay->lines = reinterpret_cast<ALfloat(*)[4]>(offset);

        // Return the sample count for accumulation.
        return samples;
    }

    // Calculates the delay line metrics and allocates the shared sample buffer
    // for all lines given the sample rate (frequency).  If an allocation failure
    // occurs, it returns AL_FALSE.
    ALboolean alloc_lines(
        const ALuint frequency)
    {
        // All delay line lengths are calculated to accomodate the full range of
        // lengths given their respective paramters.
        auto total_samples = ALuint{0};

        // Multiplier for the maximum density value, i.e. density=1, which is
        // actually the least density...
        //
        auto multiplier = 1.0f + line_multiplier;

        // The main delay length includes the maximum early reflection delay, the
        // largest early tap width, the maximum late reverb delay, and the
        // largest late tap width.  Finally, it must also be extended by the
        // update size (MAX_UPDATE_SAMPLES) for block processing.
        auto length = AL_EAXREVERB_MAX_REFLECTIONS_DELAY +
                 (early_tap_lengths[3] * multiplier) +
                 AL_EAXREVERB_MAX_LATE_REVERB_DELAY +
                 (late_line_lengths[3] - late_line_lengths[0]) * 0.25F * multiplier;

        total_samples += calc_line_length(length, total_samples, frequency, max_update_samples, &delay_);

        // The early vector all-pass line.
        length = early_allpass_lengths[3] * multiplier;
        total_samples += calc_line_length(length, total_samples, frequency, 0, &early_.vec_ap.delay);

        // The early reflection line.
        length = early_line_lengths[3] * multiplier;
        total_samples += calc_line_length(length, total_samples, frequency, 0, &early_.delay);

        // The late vector all-pass line.
        length = late_allpass_lengths[3] * multiplier;
        total_samples += calc_line_length(length, total_samples, frequency, 0, &late_.vec_ap.delay);

        // The late delay lines are calculated from the larger of the maximum
        // density line length or the maximum echo time, and includes the maximum
        // modulation-related delay. The modulator's delay is calculated from the
        // maximum modulation time and depth coefficient, and halved for the low-
        // to-high frequency swing.
        length = std::max(AL_EAXREVERB_MAX_ECHO_TIME, late_line_lengths[3] * multiplier) +
            (AL_EAXREVERB_MAX_MODULATION_TIME * modulation_depth_coeff / 2.0F);

        total_samples += calc_line_length(length, total_samples, frequency, 0, &late_.delay);

        if (total_samples != total_samples_)
        {
            sample_buffer_.resize(4 * total_samples);
            total_samples_ = total_samples;
        }

        // Update all delays to reflect the new sample buffer.
        realize_line_offset(sample_buffer_, &delay_);
        realize_line_offset(sample_buffer_, &early_.vec_ap.delay);
        realize_line_offset(sample_buffer_, &early_.delay);
        realize_line_offset(sample_buffer_, &late_.vec_ap.delay);
        realize_line_offset(sample_buffer_, &late_.delay);

        // Clear the sample buffer.
        std::fill(sample_buffer_.begin(), sample_buffer_.end(), 0.0F);

        return AL_TRUE;
    }


    //
    // Effect Update
    //

    // Calculate a decay coefficient given the length of each cycle and the time
    // until the decay reaches -60 dB.
    static ALfloat calc_decay_coeff(
        const ALfloat length,
        const ALfloat decayTime)
    {
        return std::pow(REVERB_DECAY_GAIN, length / decayTime);
    }

    // Calculate a decay length from a coefficient and the time until the decay
    // reaches -60 dB.
    static ALfloat calc_decay_length(
        const ALfloat coeff,
        const ALfloat decay_time)
    {
        return std::log10(coeff) * decay_time / std::log10(REVERB_DECAY_GAIN);
    }

    // Calculate an attenuation to be applied to the input of any echo models to
    // compensate for modal density and decay time.
    static ALfloat calc_density_gain(
        const ALfloat a)
    {
        // The energy of a signal can be obtained by finding the area under the
        // squared signal.  This takes the form of Sum(x_n^2), where x is the
        // amplitude for the sample n.
        //
        // Decaying feedback matches exponential decay of the form Sum(a^n),
        // where a is the attenuation coefficient, and n is the sample.  The area
        // under this decay curve can be calculated as:  1 / (1 - a).
        //
        // Modifying the above equation to find the area under the squared curve
        // (for energy) yields:  1 / (1 - a^2).  Input attenuation can then be
        // calculated by inverting the square root of this approximation,
        // yielding:  1 / sqrt(1 / (1 - a^2)), simplified to: sqrt(1 - a^2).
        //
        return std::sqrt(1.0F - (a * a));
    }

    // Calculate the scattering matrix coefficients given a diffusion factor.
    static void calc_matrix_coeffs(
        const ALfloat diffusion,
        ALfloat* x,
        ALfloat* y)
    {
        // The matrix is of order 4, so n is sqrt(4 - 1).
        const auto n = std::sqrt(3.0F);
        const auto t = diffusion * std::atan(n);

        // Calculate the first mixing matrix coefficient.
        *x = std::cos(t);

        // Calculate the second mixing matrix coefficient.
        *y = std::sin(t) / n;
    }

    // Calculate the limited HF ratio for use with the late reverb low-pass
    // filters.
    static ALfloat calc_limited_hf_ratio(
        const ALfloat hf_ratio,
        const ALfloat air_absorption_gain_hf,
        const ALfloat decay_time)
    {
        // Find the attenuation due to air absorption in dB (converting delay
        // time to meters using the speed of sound).  Then reversing the decay
        // equation, solve for HF ratio.  The delay length is cancelled out of
        // the equation, so it can be calculated once for all lines.
        const auto limit_ratio = 1.0F / (calc_decay_length(air_absorption_gain_hf, decay_time) *
            SPEEDOFSOUNDMETRESPERSEC);

        // Using the limit calculated above, apply the upper bound to the HF
        // ratio. Also need to limit the result to a minimum of 0.1, just like
        // the HF ratio parameter.
        return clampf(limit_ratio, 0.1F, hf_ratio);
    }

    // Calculates the first-order high-pass coefficients following the I3DL2
    // reference model.  This is the transfer function:
    //
    //                1 - z^-1
    //     H(z) = p ------------
    //               1 - p z^-1
    //
    // And this is the I3DL2 coefficient calculation given gain (g) and reference
    // angular frequency (w):
    //
    //                                    g
    //      p = ------------------------------------------------------
    //          g cos(w) + sqrt((cos(w) - 1) (g^2 cos(w) + g^2 - 2))
    //
    // The coefficient is applied to the partial differential filter equation as:
    //
    //     c_0 = p
    //     c_1 = -p
    //     c_2 = p
    //     y_i = c_0 x_i + c_1 x_(i-1) + c_2 y_(i-1)
    //
    static void calc_highpass_coeffs(
        const ALfloat gain,
        const ALfloat w,
        ALfloat coeffs[3])
    {
        if (gain >= 1.0F)
        {
            coeffs[0] = 1.0F;
            coeffs[1] = 0.0F;
            coeffs[2] = 0.0F;

            return;
        }

        const auto g = std::max(0.001F, gain);
        const auto g2 = g * g;
        const auto cw = std::cos(w);
        const auto p = g / ((g * cw) + std::sqrt((cw - 1.0F) * ((g2 * cw) + g2 - 2.0F)));

        coeffs[0] = p;
        coeffs[1] = -p;
        coeffs[2] = p;
    }

    // Calculates the first-order low-pass coefficients following the I3DL2
    // reference model.  This is the transfer function:
    //
    //              (1 - a) z^0
    //     H(z) = ----------------
    //             1 z^0 - a z^-1
    //
    // And this is the I3DL2 coefficient calculation given gain (g) and reference
    // angular frequency (w):
    //
    //          1 - g^2 cos(w) - sqrt(2 g^2 (1 - cos(w)) - g^4 (1 - cos(w)^2))
    //     a = ----------------------------------------------------------------
    //                                    1 - g^2
    //
    // The coefficient is applied to the partial differential filter equation as:
    //
    //     c_0 = 1 - a
    //     c_1 = 0
    //     c_2 = a
    //     y_i = c_0 x_i + c_1 x_(i-1) + c_2 y_(i-1)
    //
    static void calc_lowpass_coeffs(
        const ALfloat gain,
        const ALfloat w,
        ALfloat coeffs[3])
    {
        if (gain >= 1.0F)
        {
            coeffs[0] = 1.0F;
            coeffs[1] = 0.0F;
            coeffs[2] = 0.0F;

            return;
        }

        /* Be careful with gains < 0.001, as that causes the coefficient
         * to head towards 1, which will flatten the signal. */
        const auto g = std::max(0.001F, gain);
        const auto g2 = g * g;
        const auto cw = std::cos(w);

        const auto a = (1.0F - (g2 * cw) - std::sqrt((2.0F * g2 * (1.0F - cw)) - (g2 * g2 * (1.0F - (cw * cw))))) /
            (1.0F - g2);

        coeffs[0] = 1.0F - a;
        coeffs[1] = 0.0F;
        coeffs[2] = a;
    }

    // Calculates the first-order low-shelf coefficients.  The shelf filters are
    // used in place of low/high-pass filters to preserve the mid-band.  This is
    // the transfer function:
    //
    //             a_0 + a_1 z^-1
    //     H(z) = ----------------
    //              1 + b_1 z^-1
    //
    // And these are the coefficient calculations given cut gain (g) and a center
    // angular frequency (w):
    //
    //          sin(0.5 (pi - w) - 0.25 pi)
    //     p = -----------------------------
    //          sin(0.5 (pi - w) + 0.25 pi)
    //
    //          g + 1           g + 1
    //     a = ------- + sqrt((-------)^2 - 1)
    //          g - 1           g - 1
    //
    //            1 + g + (1 - g) a
    //     b_0 = -------------------
    //                    2
    //
    //            1 - g + (1 + g) a
    //     b_1 = -------------------
    //                    2
    //
    // The coefficients are applied to the partial differential filter equation
    // as:
    //
    //            b_0 + p b_1
    //     c_0 = -------------
    //              1 + p a
    //
    //            -(b_1 + p b_0)
    //     c_1 = ----------------
    //               1 + p a
    //
    //             p + a
    //     c_2 = ---------
    //            1 + p a
    //
    //     y_i = c_0 x_i + c_1 x_(i-1) + c_2 y_(i-1)
    //
    static void calc_low_shelf_coeffs(
        const ALfloat gain,
        const ALfloat w,
        ALfloat coeffs[3])
    {
        if (gain >= 1.0F)
        {
            coeffs[0] = 1.0F;
            coeffs[1] = 0.0F;
            coeffs[2] = 0.0F;

            return;
        }

        const auto g = std::max(0.001F, gain);
        const auto rw = F_PI - w;
        const auto p = std::sin((0.5F * rw) - (0.25F * F_PI)) / std::sin((0.5F * rw) + (0.25F * F_PI));
        const auto n = (g + 1.0F) / (g - 1.0F);
        const auto alpha = n + std::sqrt((n * n) - 1.0F);
        const auto beta0 = (1.0F + g + (1.0F - g) * alpha) / 2.0F;
        const auto beta1 = (1.0F - g + (1.0F + g) * alpha) / 2.0F;

        coeffs[0] = (beta0 + (p * beta1)) / (1.0F + (p * alpha));
        coeffs[1] = -(beta1 + (p * beta0)) / (1.0F + (p * alpha));
        coeffs[2] = (p + alpha) / (1.0F + (p * alpha));
    }

    // Calculates the first-order high-shelf coefficients.  The shelf filters are
    // used in place of low/high-pass filters to preserve the mid-band.  This is
    // the transfer function:
    //
    //             a_0 + a_1 z^-1
    //     H(z) = ----------------
    //              1 + b_1 z^-1
    //
    // And these are the coefficient calculations given cut gain (g) and a center
    // angular frequency (w):
    //
    //          sin(0.5 w - 0.25 pi)
    //     p = ----------------------
    //          sin(0.5 w + 0.25 pi)
    //
    //          g + 1           g + 1
    //     a = ------- + sqrt((-------)^2 - 1)
    //          g - 1           g - 1
    //
    //            1 + g + (1 - g) a
    //     b_0 = -------------------
    //                    2
    //
    //            1 - g + (1 + g) a
    //     b_1 = -------------------
    //                    2
    //
    // The coefficients are applied to the partial differential filter equation
    // as:
    //
    //            b_0 + p b_1
    //     c_0 = -------------
    //              1 + p a
    //
    //            b_1 + p b_0
    //     c_1 = -------------
    //              1 + p a
    //
    //            -(p + a)
    //     c_2 = ----------
    //            1 + p a
    //
    //     y_i = c_0 x_i + c_1 x_(i-1) + c_2 y_(i-1)
    //
    //
    static void calc_high_shelf_coeffs(
        const ALfloat gain,
        const ALfloat w,
        ALfloat coeffs[3])
    {
        if (gain >= 1.0F)
        {
            coeffs[0] = 1.0F;
            coeffs[1] = 0.0F;
            coeffs[2] = 0.0F;

            return;
        }

        const auto g = std::max(0.001F, gain);
        const auto p = std::sin((0.5F * w) - (0.25F * F_PI)) / std::sin((0.5F * w) + (0.25F * F_PI));
        const auto n = (g + 1.0F) / (g - 1.0F);
        const auto alpha = n + std::sqrt((n * n) - 1.0F);
        const auto beta0 = (1.0F + g + (1.0F - g) * alpha) / 2.0F;
        const auto beta1 = (1.0F - g + (1.0F + g) * alpha) / 2.0F;

        coeffs[0] = (beta0 + (p * beta1)) / (1.0F + (p * alpha));
        coeffs[1] = (beta1 + (p * beta0)) / (1.0F + (p * alpha));
        coeffs[2] = -(p + alpha) / (1.0F + (p * alpha));
    }

    // Calculates the 3-band T60 damping coefficients for a particular delay line
    // of specified length using a combination of two low/high-pass/shelf or
    // pass-through filter sections (producing 3 coefficients each) and a general
    // gain (7th coefficient) given decay times for each band split at two (LF/
    // HF) reference frequencies (w).
    static void calc_t60_damping_coeffs(
        const ALfloat length,
        const ALfloat lf_decay_time,
        const ALfloat mf_decay_time,
        const ALfloat hf_decay_time,
        const ALfloat lf_w,
        const ALfloat hf_w,
        ALfloat lfcoeffs[3],
        ALfloat hfcoeffs[3],
        ALfloat *midcoeff)
    {
        const auto lf_gain = calc_decay_coeff(length, lf_decay_time);
        const auto mf_gain = calc_decay_coeff(length, mf_decay_time);
        const auto hf_gain = calc_decay_coeff(length, hf_decay_time);

        if (lf_gain < mf_gain)
        {
            if (mf_gain < hf_gain)
            {
                calc_low_shelf_coeffs(mf_gain / hf_gain, hf_w, lfcoeffs);
                calc_highpass_coeffs(lf_gain / mf_gain, lf_w, hfcoeffs);
                *midcoeff = hf_gain;
            }
            else if (mf_gain > hf_gain)
            {
                calc_highpass_coeffs(lf_gain / mf_gain, lf_w, lfcoeffs);
                calc_lowpass_coeffs(hf_gain / mf_gain, hf_w, hfcoeffs);
                *midcoeff = mf_gain;
            }
            else
            {
                lfcoeffs[0] = 1.0F;
                lfcoeffs[1] = 0.0F;
                lfcoeffs[2] = 0.0F;
                calc_highpass_coeffs(lf_gain / mf_gain, lf_w, hfcoeffs);
                *midcoeff = mf_gain;
            }
        }
        else if (lf_gain > mf_gain)
        {
            if (mf_gain < hf_gain)
            {
                const auto hg = mf_gain / lf_gain;
                const auto lg = mf_gain / hf_gain;

                calc_high_shelf_coeffs(hg, lf_w, lfcoeffs);
                calc_low_shelf_coeffs(lg, hf_w, hfcoeffs);
                *midcoeff = maxf(lf_gain, hf_gain) / maxf(hg, lg);
            }
            else if (mf_gain > hf_gain)
            {
                calc_high_shelf_coeffs(mf_gain / lf_gain, lf_w, lfcoeffs);
                calc_lowpass_coeffs(hf_gain / mf_gain, hf_w, hfcoeffs);
                *midcoeff = lf_gain;
            }
            else
            {
                lfcoeffs[0] = 1.0F;
                lfcoeffs[1] = 0.0F;
                lfcoeffs[2] = 0.0F;
                calc_high_shelf_coeffs(mf_gain / lf_gain, lf_w, hfcoeffs);
                *midcoeff = lf_gain;
            }
        }
        else
        {
            lfcoeffs[0] = 1.0F;
            lfcoeffs[1] = 0.0F;
            lfcoeffs[2] = 0.0F;

            if (mf_gain < hf_gain)
            {
                calc_low_shelf_coeffs(mf_gain / hf_gain, hf_w, hfcoeffs);
                *midcoeff = hf_gain;
            }
            else if (mf_gain > hf_gain)
            {
                calc_lowpass_coeffs(hf_gain / mf_gain, hf_w, hfcoeffs);
                *midcoeff = mf_gain;
            }
            else
            {
                hfcoeffs[0] = 1.0F;
                hfcoeffs[1] = 0.0F;
                hfcoeffs[2] = 0.0F;
                *midcoeff = mf_gain;
            }
        }
    }

    // Update the EAX modulation index, range, and depth.  Keep in mind that this
    // kind of vibrato is additive and not multiplicative as one may expect.  The
    // downswing will sound stronger than the upswing.
    void update_modulator(
        const ALfloat mod_time,
        const ALfloat mod_depth,
        const ALuint frequency)
    {
        // Modulation is calculated in two parts.
        //
        // The modulation time effects the speed of the sinus. An index out of the
        // current range (both in samples) is incremented each sample, so a longer
        // time implies a larger range. The range is bound to a reasonable minimum
        // (1 sample) and when the timing changes, the index is rescaled to the new
        // range to keep the sinus consistent.
        //
        const ALuint range = std::max(fastf2i(mod_time * frequency), 1);

        mod_.index = static_cast<ALuint>(mod_.index * static_cast<uint64_t>(range) / mod_.range);
        mod_.range = range;

        // The modulation depth effects the scale of the sinus, which changes how
        // much extra delay is added to the delay line. This delay changing over
        // time changes the pitch, creating the modulation effect. The scale needs
        // to be multiplied by the modulation time so that a given depth produces a
        // consistent shift in frequency over all ranges of time. Since the depth
        // is applied to a sinus value, it needs to be halved for the sinus swing
        // in time (half of it is spent decreasing the frequency, half is spent
        // increasing it).
        mod_.depth = mod_depth * modulation_depth_coeff * mod_time / 2.0F * frequency;
    }

    // Update the offsets for the main effect delay line.
    void update_delay_line(
        const ALfloat early_delay,
        const ALfloat late_delay,
        const ALfloat density,
        const ALfloat decay_time,
        const ALuint frequency)
    {
        const auto multiplier = 1.0F + (density * line_multiplier);

        // Early reflection taps are decorrelated by means of an average room
        // reflection approximation described above the definition of the taps.
        // This approximation is linear and so the above density multiplier can
        // be applied to adjust the width of the taps.  A single-band decay
        // coefficient is applied to simulate initial attenuation and absorption.
        //
        // Late reverb taps are based on the late line lengths to allow a zero-
        // delay path and offsets that would continue the propagation naturally
        // into the late lines.

        for (int i = 0; i < 4; ++i)
        {
            auto length = ALfloat{};

            length = early_delay + (early_tap_lengths[i] * multiplier);
            early_delay_taps_[i][1] = fastf2i(length * frequency);

            length = early_tap_lengths[i] * multiplier;
            early_delay_coeffs_[i] = calc_decay_coeff(length, decay_time);

            length = late_delay + (late_line_lengths[i] - late_line_lengths[0]) * 0.25F * multiplier;
            late_delay_taps_[i][1] = late_feed_tap_ + fastf2i(length * frequency);
        }
    }

    // Update the early reflection line lengths and gain coefficients.
    void update_early_lines(
        const ALfloat density,
        const ALfloat decay_time,
        const ALuint frequency)
    {
        const auto multiplier = 1.0f + density*line_multiplier;

        for(int i = 0; i < 4; ++i)
        {
            auto length = ALfloat{};

            // Calculate the length (in seconds) of each all-pass line.
            length = early_allpass_lengths[i] * multiplier;

            // Calculate the delay offset for each all-pass line.
            early_.vec_ap.offsets[i][1] = fastf2i(length * frequency);

            // Calculate the length (in seconds) of each delay line.
            length = early_line_lengths[i] * multiplier;

            // Calculate the delay offset for each delay line.
            early_.offsets[i][1] = fastf2i(length * frequency);

            /* Calculate the gain (coefficient) for each line. */
            early_.coeffs[i] = calc_decay_coeff(length, decay_time);
        }
    }

    // Update the late reverb line lengths and T60 coefficients.
    void update_late_lines(
        const ALfloat density,
        const ALfloat diffusion,
        const ALfloat lf_decay_time,
        const ALfloat mf_decay_time,
        const ALfloat hf_decay_time,
        const ALfloat lf_w,
        const ALfloat hf_w,
        const ALfloat echo_time,
        const ALfloat echo_depth,
        const ALuint frequency)
    {
        ALfloat band_weights[3];

        // To compensate for changes in modal density and decay time of the late
        // reverb signal, the input is attenuated based on the maximal energy of
        // the outgoing signal.  This approximation is used to keep the apparent
        // energy of the signal equal for all ranges of density and decay time.
        //
        // The average length of the delay lines is used to calculate the
        // attenuation coefficient.

        const auto multiplier = 1.0F + (density * line_multiplier);

        auto length = (late_line_lengths[0] + late_line_lengths[1] +
                  late_line_lengths[2] + late_line_lengths[3]) / 4.0F * multiplier;

        // Include the echo transformation (see below).
        length = lerp(length, echo_time, echo_depth);

        length += (late_allpass_lengths[0] + late_allpass_lengths[1] +
                   late_allpass_lengths[2] + late_allpass_lengths[3]) / 4.0F * multiplier;

        // The density gain calculation uses an average decay time weighted by
        // approximate bandwidth.  This attempts to compensate for losses of
        // energy that reduce decay time due to scattering into highly attenuated
        // bands.

        band_weights[0] = lf_w;
        band_weights[1] = hf_w - lf_w;
        band_weights[2] = F_TAU - hf_w;

        late_.density_gain = calc_density_gain(
            calc_decay_coeff(
                length,
                ((band_weights[0] * lf_decay_time) +
                    (band_weights[1] * mf_decay_time) +
                    (band_weights[2] * hf_decay_time)) / F_TAU)
        );

        for (int i = 0; i < 4; ++i)
        {
            // Calculate the length (in seconds) of each all-pass line.
            length = late_allpass_lengths[i] * multiplier;

            // Calculate the delay offset for each all-pass line.
            late_.vec_ap.offsets[i][1] = fastf2i(length * frequency);

            // Calculate the length (in seconds) of each delay line.  This also
            // applies the echo transformation.  As the EAX echo depth approaches
            // 1, the line lengths approach a length equal to the echoTime.  This
            // helps to produce distinct echoes along the tail.
            length = lerp(late_line_lengths[i] * multiplier, echo_time, echo_depth);

            // Calculate the delay offset for each delay line.
            late_.offsets[i][1] = fastf2i(length * frequency);

            // Approximate the absorption that the vector all-pass would exhibit
            // given the current diffusion so we don't have to process a full T60
            // filter for each of its four lines.
            length += lerp(late_allpass_lengths[i],
                (late_allpass_lengths[0] + late_allpass_lengths[1] +
                    late_allpass_lengths[2] + late_allpass_lengths[3]) / 4.0F,
                diffusion) * multiplier;

            // Calculate the T60 damping coefficients for each line.
            calc_t60_damping_coeffs(
                length,
                lf_decay_time,
                mf_decay_time,
                hf_decay_time,
                lf_w,
                hf_w,
                late_.filters[i].lf_coeffs,
                late_.filters[i].hf_coeffs,
                &late_.filters[i].mid_coeff);
        }
    }

    static void matrix_mult(
        aluMatrixf& res,
        const aluMatrixf& m1,
        const aluMatrixf& m2)
    {
        for (int col = 0; col < 4; ++col)
        {
            for (int row = 0;row < 4; ++row)
            {
                res.m[row][col] =
                    (m1.m[row][0] * m2.m[0][col]) +
                    (m1.m[row][1] * m2.m[1][col]) +
                    (m1.m[row][2] * m2.m[2][col]) +
                    (m1.m[row][3] * m2.m[3][col]);
            }
        }
    }

    // Creates a transform matrix given a reverb vector. This works by creating a
    // Z-focus transform, then a rotate transform around X, then Y, to place the
    // focal point in the direction of the vector, using the vector length as a
    // focus strength.
    //
    // This isn't technically correct since the vector is supposed to define the
    // aperture and not rotate the perceived soundfield, but in practice it's
    // probably good enough.
    static aluMatrixf get_transform_from_vector(
        const ALfloat* vec)
    {
        aluMatrixf zfocus;
        aluMatrixf xrot;
        aluMatrixf yrot;
        aluMatrixf tmp1;
        aluMatrixf tmp2;

        const auto length = std::sqrt((vec[0]*vec[0]) + (vec[1]*vec[1]) + (vec[2]*vec[2]));

        // Define a Z-focus (X in Ambisonics) transform, given the panning vector
        // length.
        const auto sa = std::sin(std::min(length, 1.0F) * (F_PI / 4.0F));

        aluMatrixfSet(
            &zfocus,
            1.0F / (1.0F + sa), 0.0F, 0.0F, (sa / (1.0F + sa)) / 1.732050808F,
            0.0F, std::sqrt((1.0F - sa) / (1.0F + sa)), 0.0F, 0.0F,
            0.0F, 0.0F, std::sqrt((1.0F - sa) / (1.0F + sa)), 0.0F,
            (sa / (1.0F + sa)) * 1.732050808F, 0.0F, 0.0F, 1.0F / (1.0F + sa)
        );

        // Define rotation around X (Y in Ambisonics)
        auto a = std::atan2(vec[1], std::sqrt((vec[0] * vec[0]) + (vec[2] * vec[2])));

        aluMatrixfSet(
            &xrot,
            1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F,  std::cos(a), std::sin(a),
            0.0F, 0.0F, -std::sin(a), std::cos(a)
        );

        // Define rotation around Y (Z in Ambisonics). NOTE: EFX's reverb vectors
        // use a right-handled coordinate system, compared to the rest of OpenAL
        // which uses left-handed. This is fixed by negating Z, however it would
        // need to also be negated to get a proper Ambisonics angle, thus
        // cancelling it out.
        a = std::atan2(-vec[0], vec[2]);

        aluMatrixfSet(
            &yrot,
            1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, std::cos(a), 0.0F, std::sin(a),
            0.0F, 0.0F, 1.0F, 0.0F,
            0.0F, -std::sin(a), 0.0F, std::cos(a)
        );

        // Define a matrix that first focuses on Z, then rotates around X then Y to
        // focus the output in the direction of the vector.
        matrix_mult(tmp1, xrot, zfocus);
        matrix_mult(tmp2, yrot, tmp1);

        return tmp2;
    }

    // Note: res is transposed.
    static void matrix_mult_t(
        aluMatrixf& res,
        const aluMatrixf& m1,
        const aluMatrixf& m2)
    {
        for (int col = 0; col < 4; ++col)
        {
            for (int row = 0; row < 4; ++row)
            {
                res.m[col][row] =
                    (m1.m[row][0] * m2.m[0][col]) +
                    (m1.m[row][1] * m2.m[1][col]) +
                    (m1.m[row][2] * m2.m[2][col]) +
                    (m1.m[row][3] * m2.m[3][col]);
            }
        }
    }

    /* Update the early and late 3D panning gains. */
    void update_3d_panning(
        const ALCdevice* device,
        const ALfloat* reflections_pan,
        const ALfloat* late_reverb_pan,
        const ALfloat gain,
        const ALfloat early_gain,
        const ALfloat late_gain)
    {
        aluMatrixf transform;
        aluMatrixf rot;

        out_buffer = device->foa_out.buffer;
        out_channels = device->foa_out.num_channels;

        // Create a matrix that first converts A-Format to B-Format, then rotates
        // the B-Format soundfield according to the panning vector.
        rot = get_transform_from_vector(reflections_pan);
        matrix_mult_t(transform, rot, a2b);
        memset(&early_.pan_gains, 0, sizeof(early_.pan_gains));

        for (int i = 0; i < MAX_EFFECT_CHANNELS; ++i)
        {
            ComputeFirstOrderGains(device->foa_out, transform.m[i], gain*early_gain, early_.pan_gains[i]);
        }

        rot = get_transform_from_vector(late_reverb_pan);
        matrix_mult_t(transform, rot, a2b);
        memset(&late_.pan_gains, 0, sizeof(late_.pan_gains));

        for (int i = 0; i < MAX_EFFECT_CHANNELS; ++i)
        {
            ComputeFirstOrderGains(device->foa_out, transform.m[i], gain*late_gain, late_.pan_gains[i]);
        }
    }


    /**************************************
     *  Effect Processing                 *
     **************************************/

    // Basic delay line input/output routines.
    static ALfloat delay_line_out(
        const DelayLineI* delay,
        const ALsizei offset,
        const ALsizei c)
    {
        return delay->lines[offset & delay->mask][c];
    }

    // Cross-faded delay line output routine.  Instead of interpolating the
    // offsets, this interpolates (cross-fades) the outputs at each offset.
    static ALfloat faded_delay_line_out(
        const DelayLineI* delay,
        const ALsizei off0,
        const ALsizei off1,
        const ALsizei c,
        const ALfloat mu)
    {
        return lerp(delay->lines[off0 & delay->mask][c], delay->lines[off1 & delay->mask][c], mu);
    }

    static ALfloat delay_out_faded(
        const DelayLineI* delay,
        const ALsizei off0,
        const ALsizei off1,
        const ALsizei c,
        const ALfloat mu)
    {
        return faded_delay_line_out(delay, off0, off1, c, mu);
    }

    static ALfloat delay_out_unfaded(
        const DelayLineI* delay,
        const ALsizei off0,
        const ALsizei off1,
        const ALsizei c,
        const ALfloat mu)
    {
        static_cast<void>(off1);
        static_cast<void>(mu);

        return delay_line_out(delay, off0, c);
    }

    using DelayOutFunc = ALfloat (*)(
        const DelayLineI* delay,
        const ALsizei off0,
        const ALsizei off1,
        const ALsizei c,
        const ALfloat mu);

    static void delay_line_in(
        DelayLineI* delay,
        const ALsizei offset,
        const ALsizei c,
        const ALfloat in)
    {
        delay->lines[offset & delay->mask][c] = in;
    }

    static void delay_line_in4(
        DelayLineI* delay,
        ALsizei offset,
        const ALfloat in[4])
    {
        offset &= delay->mask;

        for (int i = 0; i < 4; ++i)
        {
            delay->lines[offset][i] = in[i];
        }
    }

    static void delay_line_in4_rev(
        DelayLineI* delay,
        ALsizei offset,
        const ALfloat in[4])
    {
        offset &= delay->mask;

        for (int i = 0; i < 4; ++i)
        {
            delay->lines[offset][i] = in[3 - i];
        }
    }

    void calc_modulation_delays(
        ALint* delays,
        const ALsizei todo)
    {
        auto index = mod_.index;
        auto range = mod_.filter;

        for (int i = 0; i < todo; ++i)
        {
            // Calculate the sinus rhythm (dependent on modulation time and the
            // sampling rate).
            const auto sinus = std::sin(F_TAU * index / mod_.range);

            // Step the modulation index forward, keeping it bound to its range.
            index = (index + 1) % mod_.range;

            // The depth determines the range over which to read the input samples
            // from, so it must be filtered to reduce the distortion caused by even
            // small parameter changes.
            range = lerp(range, mod_.depth, mod_.coeff);

            // Calculate the read offset.
            delays[i] = std::lround(range * sinus);
        }

        mod_.index = index;
        mod_.filter = range;
    }

    // Applies a scattering matrix to the 4-line (vector) input.  This is used
    // for both the below vector all-pass model and to perform modal feed-back
    // delay network (FDN) mixing.
    //
    // The matrix is derived from a skew-symmetric matrix to form a 4D rotation
    // matrix with a single unitary rotational parameter:
    //
    //     [  d,  a,  b,  c ]          1 = a^2 + b^2 + c^2 + d^2
    //     [ -a,  d,  c, -b ]
    //     [ -b, -c,  d,  a ]
    //     [ -c,  b, -a,  d ]
    //
    // The rotation is constructed from the effect's diffusion parameter,
    // yielding:
    //
    //     1 = x^2 + 3 y^2
    //
    // Where a, b, and c are the coefficient y with differing signs, and d is the
    // coefficient x.  The final matrix is thus:
    //
    //     [  x,  y, -y,  y ]          n = sqrt(matrix_order - 1)
    //     [ -y,  x,  y,  y ]          t = diffusion_parameter * atan(n)
    //     [  y, -y,  x,  y ]          x = cos(t)
    //     [ -y, -y, -y,  x ]          y = sin(t) / n
    //
    // Any square orthogonal matrix with an order that is a power of two will
    // work (where ^T is transpose, ^-1 is inverse):
    //
    //     M^T = M^-1
    //
    // Using that knowledge, finding an appropriate matrix can be accomplished
    // naively by searching all combinations of:
    //
    //     M = D + S - S^T
    //
    // Where D is a diagonal matrix (of x), and S is a triangular matrix (of y)
    // whose combination of signs are being iterated.
    //
    static void vector_partial_scatter(
        ALfloat* vec,
        const ALfloat x_coeff,
        const ALfloat y_coeff)
    {
        const ALfloat f[] = {vec[0], vec[1], vec[2], vec[3],};

        vec[0] = (x_coeff * f[0]) + (y_coeff * (f[1] + -f[2] + f[3]));
        vec[1] = (x_coeff * f[1]) + (y_coeff * (-f[0] + f[2] + f[3]));
        vec[2] = (x_coeff * f[2]) + (y_coeff * (f[0] + -f[1] + f[3]));
        vec[3] = (x_coeff * f[3]) + (y_coeff * (-f[0] + -f[1] + -f[2]));
    }

    // This applies a Gerzon multiple-in/multiple-out (MIMO) vector all-pass
    // filter to the 4-line input.
    //
    // It works by vectorizing a regular all-pass filter and replacing the delay
    // element with a scattering matrix (like the one above) and a diagonal
    // matrix of delay elements.
    //
    // Two static specializations are used for transitional (cross-faded) delay
    // line processing and non-transitional processing.
    static void vector_allpass_x(
        DelayOutFunc delay_out_func,
        ALfloat* vec,
        const ALsizei offset,
        const ALfloat feed_coeff,
        const ALfloat x_coeff,
        const ALfloat y_coeff,
        const ALfloat mu,
        VecAllpass *vap)
    {
        ALfloat f[4];

        for (int i = 0; i < 4; i++)
        {
            auto input = vec[i];

            vec[i] = delay_out_func(
                &vap->delay,
                offset - vap->offsets[i][0],
                offset - vap->offsets[i][1],
                i,
                mu) - (feed_coeff * input);

            f[i] = input + (feed_coeff * vec[i]);
        }

        vector_partial_scatter(f, x_coeff, y_coeff);

        delay_line_in4(&vap->delay, offset, f);
    }

    static void vector_allpass_unfaded(
        ALfloat* vec,
        const ALsizei offset,
        const ALfloat feed_coeff,
        const ALfloat x_coeff,
        const ALfloat y_coeff,
        const ALfloat mu,
        VecAllpass* vap)
    {
        vector_allpass_x(delay_out_unfaded, vec, offset, feed_coeff, x_coeff, y_coeff, mu, vap);
    }

    static void vector_allpass_faded(
        ALfloat* vec,
        const ALsizei offset,
        const ALfloat feed_coeff,
        const ALfloat x_coeff,
        const ALfloat y_coeff,
        const ALfloat mu,
        VecAllpass* vap)
    {
        vector_allpass_x(delay_out_faded, vec, offset, feed_coeff, x_coeff, y_coeff, mu, vap);
    }

    // A helper to reverse vector components.
    static void vector_reverse(
        ALfloat vec[4])
    {
        std::swap(vec[0], vec[3]);
        std::swap(vec[1], vec[2]);
    }


    using VectorAllpassFunc = void (*)(
        ALfloat* vec,
        const ALsizei offset,
        const ALfloat feed_coeff,
        const ALfloat x_coeff,
        const ALfloat y_coeff,
        const ALfloat mu,
        VecAllpass* vap);

    // This generates early reflections.
    //
    // This is done by obtaining the primary reflections (those arriving from the
    // same direction as the source) from the main delay line.  These are
    // attenuated and all-pass filtered (based on the diffusion parameter).
    //
    // The early lines are then fed in reverse (according to the approximately
    // opposite spatial location of the A-Format lines) to create the secondary
    // reflections (those arriving from the opposite direction as the source).
    //
    // The early response is then completed by combining the primary reflections
    // with the delayed and attenuated output from the early lines.
    //
    // Finally, the early response is reversed, scattered (based on diffusion),
    // and fed into the late reverb section of the main delay line.
    //
    // Two static specializations are used for transitional (cross-faded) delay
    // line processing and non-transitional processing.
    //
    void early_reflection_x(
        VectorAllpassFunc vector_allpass_func,
        DelayOutFunc delay_out_func,
        const ALsizei todo,
        ALfloat fade,
        ALfloat (*out)[max_update_samples])
    {
        ALfloat f[4];
        auto current_offset = offset_;

        for (int i = 0; i < todo; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                f[j] = delay_out_func(
                    &delay_,
                    current_offset - early_delay_taps_[j][0],
                    current_offset - early_delay_taps_[j][1],
                    j, fade) * early_delay_coeffs_[j];
            }

            vector_allpass_func(f, current_offset, ap_feed_coeff_, mix_x_, mix_y_, fade, &early_.vec_ap);

            delay_line_in4_rev(&early_.delay, current_offset, f);

            for (int j = 0; j < 4; ++j)
            {
                f[j] += delay_out_func(&early_.delay,
                    current_offset - early_.offsets[j][0],
                    current_offset - early_.offsets[j][1], j, fade
                ) * early_.coeffs[j];
            }

            for (int j = 0; j < 4; ++j)
            {
                out[j][i] = f[j];
            }

            vector_reverse(f);

            vector_partial_scatter(f, mix_x_, mix_y_);

            delay_line_in4(&delay_, current_offset - late_feed_tap_, f);

            current_offset += 1;
            fade += fade_step;
        }
    }

    void early_reflection_unfaded(
        const ALsizei todo,
        ALfloat fade,
        ALfloat (*out)[max_update_samples])
    {
        early_reflection_x(vector_allpass_unfaded, delay_out_unfaded, todo, fade, out);
    }

    void early_reflection_faded(
        const ALsizei todo,
        ALfloat fade,
        ALfloat (*out)[max_update_samples])
    {
        early_reflection_x(vector_allpass_faded, delay_out_faded, todo, fade, out);
    }

    // Applies a first order filter section.
    static ALfloat first_order_filter(
        const ALfloat in,
        const ALfloat coeffs[3],
        ALfloat state[2])
    {
        const auto out = (coeffs[0] * in) + (coeffs[1] * state[0]) + (coeffs[2] * state[1]);

        state[0] = in;
        state[1] = out;

        return out;
    }

    // Applies the two T60 damping filter sections.
    ALfloat late_t60_filter(
        const ALsizei index,
        const ALfloat in)
    {
        ALfloat out = first_order_filter(
            in,
            late_.filters[index].lf_coeffs,
            late_.filters[index].states[0]);

        return late_.filters[index].mid_coeff *
            first_order_filter(out, late_.filters[index].hf_coeffs,
                late_.filters[index].states[1]);
    }

    // This generates the reverb tail using a modified feed-back delay network
    // (FDN).
    //
    // Results from the early reflections are attenuated by the density gain and
    // mixed with the output from the late delay lines.
    //
    // The late response is then completed by T60 and all-pass filtering the mix.
    //
    // Finally, the lines are reversed (so they feed their opposite directions)
    // and scattered with the FDN matrix before re-feeding the delay lines.
    //
    // Two static specializations are used for transitional (cross-faded) delay
    // line processing and non-transitional processing.
    //
    void late_reverb_x(
        VectorAllpassFunc vector_allpass_func,
        DelayOutFunc delay_out_func,
        const ALsizei todo,
        ALfloat fade,
        ALfloat(*out)[max_update_samples])
    {
        ALfloat f[4];
        ALint moddelay[max_update_samples];

        calc_modulation_delays(moddelay, todo);

        auto current_offset = offset_;

        for (int i = 0; i < todo; i++)
        {
            for (int j = 0; j < 4; ++j)
            {
                f[j] = delay_out_func(
                    &delay_,
                    current_offset - late_delay_taps_[j][0],
                    current_offset - late_delay_taps_[j][1],
                    j,
                    fade
                ) * late_.density_gain;
            }

            const auto current_delay = current_offset - moddelay[i];

            for (int j = 0; j < 4; ++j)
            {
                f[j] += delay_out_func(&late_.delay,
                    current_delay - late_.offsets[j][0],
                    current_delay - late_.offsets[j][1],
                    j,
                    fade);
            }

            for (int j = 0; j < 4; ++j)
            {
                f[j] = late_t60_filter(j, f[j]);
            }

            vector_allpass_func(f, current_offset, ap_feed_coeff_, mix_x_, mix_y_, fade, &late_.vec_ap);

            for (int j = 0; j < 4; ++j)
            {
                out[j][i] = f[j];
            }

            vector_reverse(f);

            vector_partial_scatter(f, mix_x_, mix_y_);

            delay_line_in4(&late_.delay, current_offset, f);

            current_offset += 1;
            fade += fade_step;
        }
    }

    void late_reverb_unfaded(
        const ALsizei todo,
        ALfloat fade,
        ALfloat (*out)[max_update_samples])
    {
        late_reverb_x(vector_allpass_unfaded, delay_out_unfaded, todo, fade, out);
    }

    void late_reverb_faded(
        const ALsizei todo,
        ALfloat fade,
        ALfloat (*out)[max_update_samples])
    {
        late_reverb_x(vector_allpass_faded, delay_out_faded, todo, fade, out);
    }

    // Perform the non-EAX reverb pass on a given input sample, resulting in
    // four-channel output.
    ALfloat verb_pass(
        const ALsizei todo,
        ALfloat fade,
        const ALfloat(*input)[max_update_samples],
        ALfloat(*early)[max_update_samples],
        ALfloat(*late)[max_update_samples])
    {
        for (int c = 0; c < 4; ++c)
        {
            // Low-pass filter the incoming samples (use the early buffer as temp
            // storage).
            ALfilterState_processC(&filters_[c].lp, &early[0][0], input[c], todo);

            // Feed the initial delay line.
            for (int i = 0; i < todo; ++i)
            {
                delay_line_in(&delay_, offset_ + i, c, early[0][i]);
            }
        }

        if (fade < 1.0F)
        {
            // Generate early reflections.
            early_reflection_faded(todo, fade, early);

            // Generate late reverb.
            late_reverb_faded(todo, fade, late);
            fade = minf(1.0f, fade + todo*fade_step);
        }
        else
        {
            // Generate early reflections.
            early_reflection_unfaded(todo, fade, early);

            // Generate late reverb.
            late_reverb_unfaded(todo, fade, late);
        }

        // Step all delays forward one sample.
        offset_ += todo;

        return fade;
    }

    // Perform the EAX reverb pass on a given input sample, resulting in four-
    // channel output.
    ALfloat eax_verb_pass(
        const ALsizei todo,
        ALfloat fade,
        const ALfloat(*input)[max_update_samples],
        ALfloat(*early)[max_update_samples],
        ALfloat(*late)[max_update_samples])
    {
        for (int c = 0; c < 4; ++c)
        {
            // Band-pass the incoming samples. Use the early output lines for temp
            // storage.
            ALfilterState_processC(&filters_[c].lp, early[0], input[c], todo);
            ALfilterState_processC(&filters_[c].hp, early[1], early[0], todo);

            // Feed the initial delay line.
            for (int i = 0; i < todo; i++)
            {
                delay_line_in(&delay_, offset_ + i, c, early[1][i]);
            }
        }

        if (fade < 1.0F)
        {
            // Generate early reflections.
            early_reflection_faded(todo, fade, early);

            // Generate late reverb.
            late_reverb_faded(todo, fade, late);
            fade = std::min(1.0F, fade + (todo * fade_step));
        }
        else
        {
            // Generate early reflections.
            early_reflection_unfaded(todo, fade, early);

            // Generate late reverb.
            late_reverb_unfaded(todo, fade, late);
        }

        // Step all delays forward.
        offset_ += todo;

        return fade;
    }
}; // ReverbEffect


IEffect* create_reverb_effect()
{
    return create_effect<ReverbEffect>();
}
