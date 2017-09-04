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
#include <vector>
#include "config.h"
#include "alu.h"
#include "mixer_defs.h"


/* This is the maximum number of samples processed for each inner loop
 * iteration. */
constexpr auto MAX_UPDATE_SAMPLES = 256;

/* The number of samples used for cross-faded delay lines.  This can be used
 * to balance the compensation for abrupt line changes and attenuation due to
 * minimally lengthed recursive lines.  Try to keep this below the device
 * update size.
 */
constexpr auto FADE_SAMPLES = 128;

static MixerFunc MixSamples = Mix_C;
static RowMixerFunc MixRowSamples = MixRow_C;

struct DelayLineI
{
    // The delay lines use interleaved samples, with the lengths being powers
    // of 2 to allow the use of bit-masking instead of a modulus for wrapping.

    ALsizei  mask;
    ALfloat (*lines)[4];
}; // DelayLineI

struct VecAllpass
{
    using Offsets = ALsizei[4][2];

    DelayLineI delay;
    Offsets offsets;
}; // VecAllpass

using ReverbSampleBuffer = EffectSampleBuffer;


class ReverbEffect :
    public IEffect
{
public:
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
    using Samples = ALfloat[4][MAX_UPDATE_SAMPLES];


    ReverbEffect()
        :
        IEffect{},
        is_eax{},
        sample_buffer{},
        total_samples{},
        filters{},
        delay{},
        early_delay_taps{},
        early_delay_coeffs{},
        late_feed_tap{},
        late_delay_taps{},
        ap_feed_coeff{},
        mix_x{},
        mix_y{},
        early{},
        mod{},
        late{},
        fade_count{},
        offset{},
        a_format_samples{},
        reverb_samples{},
        early_samples{}
    {
    }

    virtual ~ReverbEffect()
    {
    }

    ALboolean is_eax;


    // All delay lines are allocated as a single buffer to reduce memory
    // fragmentation and management code.
    //
    ReverbSampleBuffer sample_buffer;
    ALuint total_samples;

    // Master effect filters
    Filters filters;

    // Core delay line (early reflections and late reverb tap from this).
    DelayLineI delay;

    // Tap points for early reflection delay.
    Taps early_delay_taps;
    ALfloat early_delay_coeffs[4];

    // Tap points for late reverb feed and delay.
    ALsizei late_feed_tap;
    Taps late_delay_taps;

    // The feed-back and feed-forward all-pass coefficient.
    ALfloat ap_feed_coeff;

    // Coefficients for the all-pass and line scattering matrices.
    ALfloat mix_x;
    ALfloat mix_y;

    Early early;
    Mod mod; // EAX only
    Late late;

    // Indicates the cross-fade point for delay line reads [0,FADE_SAMPLES].
    ALsizei fade_count;

    // The current write offset for all delay lines.
    ALsizei offset;

    // Temporary storage used when processing.
    Samples a_format_samples;
    Samples reverb_samples;
    Samples early_samples;


protected:
    void do_construct() final;

    void do_destruct() final;

    ALboolean do_update_device(
        ALCdevice* device) final;

    void do_update(
        ALCdevice* device,
        const struct ALeffectslot* slot,
        const union ALeffectProps *props) final;

    void do_process(
        const ALsizei sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const ALsizei channel_count) final;
}; // ReverbEffect


/* The B-Format to A-Format conversion matrix. The arrangement of rows is
 * deliberately chosen to align the resulting lines to their spatial opposites
 * (0:above front left <-> 3:above back right, 1:below front right <-> 2:below
 * back left). It's not quite opposite, since the A-Format results in a
 * tetrahedron, but it's close enough. Should the model be extended to 8-lines
 * in the future, true opposites can be used.
 */
static const aluMatrixf B2A = {{
    { 0.288675134595f,  0.288675134595f,  0.288675134595f,  0.288675134595f },
    { 0.288675134595f, -0.288675134595f, -0.288675134595f,  0.288675134595f },
    { 0.288675134595f,  0.288675134595f, -0.288675134595f, -0.288675134595f },
    { 0.288675134595f, -0.288675134595f,  0.288675134595f, -0.288675134595f }
}};

/* Converts A-Format to B-Format. */
static const aluMatrixf A2B = {{
    { 0.866025403785f,  0.866025403785f,  0.866025403785f,  0.866025403785f },
    { 0.866025403785f, -0.866025403785f,  0.866025403785f, -0.866025403785f },
    { 0.866025403785f, -0.866025403785f, -0.866025403785f,  0.866025403785f },
    { 0.866025403785f,  0.866025403785f, -0.866025403785f, -0.866025403785f }
}};

static const ALfloat FadeStep = 1.0f / FADE_SAMPLES;

/* The all-pass and delay lines have a variable length dependent on the
 * effect's density parameter.  The resulting density multiplier is:
 *
 *     multiplier = 1 + (density * LINE_MULTIPLIER)
 *
 * Thus the line multiplier below will result in a maximum density multiplier
 * of 10.
 */
static const ALfloat LINE_MULTIPLIER = 9.0f;

/* All delay line lengths are specified in seconds.
 *
 * To approximate early reflections, we break them up into primary (those
 * arriving from the same direction as the source) and secondary (those
 * arriving from the opposite direction).
 *
 * The early taps decorrelate the 4-channel signal to approximate an average
 * room response for the primary reflections after the initial early delay.
 *
 * Given an average room dimension (d_a) and the speed of sound (c) we can
 * calculate the average reflection delay (r_a) regardless of listener and
 * source positions as:
 *
 *     r_a = d_a / c
 *     c   = 343.3
 *
 * This can extended to finding the average difference (r_d) between the
 * maximum (r_1) and minimum (r_0) reflection delays:
 *
 *     r_0 = 2 / 3 r_a
 *         = r_a - r_d / 2
 *         = r_d
 *     r_1 = 4 / 3 r_a
 *         = r_a + r_d / 2
 *         = 2 r_d
 *     r_d = 2 / 3 r_a
 *         = r_1 - r_0
 *
 * As can be determined by integrating the 1D model with a source (s) and
 * listener (l) positioned across the dimension of length (d_a):
 *
 *     r_d = int_(l=0)^d_a (int_(s=0)^d_a |2 d_a - 2 (l + s)| ds) dl / c
 *
 * The initial taps (T_(i=0)^N) are then specified by taking a power series
 * that ranges between r_0 and half of r_1 less r_0:
 *
 *     R_i = 2^(i / (2 N - 1)) r_d
 *         = r_0 + (2^(i / (2 N - 1)) - 1) r_d
 *         = r_0 + T_i
 *     T_i = R_i - r_0
 *         = (2^(i / (2 N - 1)) - 1) r_d
 *
 * Assuming an average of 5m (up to 50m with the density multiplier), we get
 * the following taps:
 */
static const ALfloat EARLY_TAP_LENGTHS[4] =
{
    0.000000e+0f, 1.010676e-3f, 2.126553e-3f, 3.358580e-3f
};

/* The early all-pass filter lengths are based on the early tap lengths:
 *
 *     A_i = R_i / a
 *
 * Where a is the approximate maximum all-pass cycle limit (20).
 */
static const ALfloat EARLY_ALLPASS_LENGTHS[4] =
{
    4.854840e-4f, 5.360178e-4f, 5.918117e-4f, 6.534130e-4f
};

/* The early delay lines are used to transform the primary reflections into
 * the secondary reflections.  The A-format is arranged in such a way that
 * the channels/lines are spatially opposite:
 *
 *     C_i is opposite C_(N-i-1)
 *
 * The delays of the two opposing reflections (R_i and O_i) from a source
 * anywhere along a particular dimension always sum to twice its full delay:
 *
 *     2 r_a = R_i + O_i
 *
 * With that in mind we can determine the delay between the two reflections
 * and thus specify our early line lengths (L_(i=0)^N) using:
 *
 *     O_i = 2 r_a - R_(N-i-1)
 *     L_i = O_i - R_(N-i-1)
 *         = 2 (r_a - R_(N-i-1))
 *         = 2 (r_a - T_(N-i-1) - r_0)
 *         = 2 r_a (1 - (2 / 3) 2^((N - i - 1) / (2 N - 1)))
 *
 * Using an average dimension of 5m, we get:
 */
static const ALfloat EARLY_LINE_LENGTHS[4] =
{
    2.992520e-3f, 5.456575e-3f, 7.688329e-3f, 9.709681e-3f
};

/* The late all-pass filter lengths are based on the late line lengths:
 *
 *     A_i = (5 / 3) L_i / r_1
 */
static const ALfloat LATE_ALLPASS_LENGTHS[4] =
{
    8.091400e-4f, 1.019453e-3f, 1.407968e-3f, 1.618280e-3f
};

/* The late lines are used to approximate the decaying cycle of recursive
 * late reflections.
 *
 * Splitting the lines in half, we start with the shortest reflection paths
 * (L_(i=0)^(N/2)):
 *
 *     L_i = 2^(i / (N - 1)) r_d
 *
 * Then for the opposite (longest) reflection paths (L_(i=N/2)^N):
 *
 *     L_i = 2 r_a - L_(i-N/2)
 *         = 2 r_a - 2^((i - N / 2) / (N - 1)) r_d
 *
 * For our 5m average room, we get:
 */
static const ALfloat LATE_LINE_LENGTHS[4] =
{
    9.709681e-3f, 1.223343e-2f, 1.689561e-2f, 1.941936e-2f
};

/* This coefficient is used to define the sinus depth according to the
 * modulation depth property. This value must be below half the shortest late
 * line length (0.0097/2 = ~0.0048), otherwise with certain parameters (high
 * mod time, low density) the downswing can sample before the input.
 */
static const ALfloat MODULATION_DEPTH_COEFF = 1.0f / 4096.0f;

/* A filter is used to avoid the terrible distortion caused by changing
 * modulation time and/or depth.  To be consistent across different sample
 * rates, the coefficient must be raised to a constant divided by the sample
 * rate:  coeff^(constant / rate).
 */
static const ALfloat MODULATION_FILTER_COEFF = 0.048f;
static const ALfloat MODULATION_FILTER_CONST = 100000.0f;


/**************************************
 *  Device Update                     *
 **************************************/

/* Given the allocated sample buffer, this function updates each delay line
 * offset.
 */
static inline ALvoid RealizeLineOffset(
    ReverbSampleBuffer& sample_buffer,
    DelayLineI* delay)
{
    auto ptr1 = &sample_buffer[reinterpret_cast<intptr_t>(delay->lines) * 4];
    auto ptr2 = reinterpret_cast<ALfloat (*)[4]>(ptr1);

    delay->lines = ptr2;
}

/* Calculate the length of a delay line and store its mask and offset. */
static ALuint CalcLineLength(
    const ALfloat length,
    const ptrdiff_t offset,
    const ALuint frequency,
    const ALuint extra,
    DelayLineI *delay)
{
    ALuint samples;

    /* All line lengths are powers of 2, calculated from their lengths in
     * seconds, rounded up.
     */
    samples = fastf2i(ceilf(length*frequency));
    samples = NextPowerOf2(samples + extra);

    /* All lines share a single sample buffer. */
    delay->mask = samples - 1;
    delay->lines = (ALfloat(*)[4])offset;

    /* Return the sample count for accumulation. */
    return samples;
}

/* Calculates the delay line metrics and allocates the shared sample buffer
 * for all lines given the sample rate (frequency).  If an allocation failure
 * occurs, it returns AL_FALSE.
 */
static ALboolean AllocLines(
    const ALuint frequency,
    ReverbEffect *state)
{
    ALuint total_samples;
    ALfloat multiplier, length;

    /* All delay line lengths are calculated to accomodate the full range of
     * lengths given their respective paramters.
     */
    total_samples = 0;

    /* Multiplier for the maximum density value, i.e. density=1, which is
     * actually the least density...
     */
    multiplier = 1.0f + LINE_MULTIPLIER;

    /* The main delay length includes the maximum early reflection delay, the
     * largest early tap width, the maximum late reverb delay, and the
     * largest late tap width.  Finally, it must also be extended by the
     * update size (MAX_UPDATE_SAMPLES) for block processing.
     */
    length = AL_EAXREVERB_MAX_REFLECTIONS_DELAY +
             EARLY_TAP_LENGTHS[3]*multiplier +
             AL_EAXREVERB_MAX_LATE_REVERB_DELAY +
             (LATE_LINE_LENGTHS[3] - LATE_LINE_LENGTHS[0])*0.25f*multiplier;
    total_samples += CalcLineLength(length, total_samples, frequency, MAX_UPDATE_SAMPLES,
                                   &state->delay);

    /* The early vector all-pass line. */
    length = EARLY_ALLPASS_LENGTHS[3] * multiplier;
    total_samples += CalcLineLength(length, total_samples, frequency, 0,
                                   &state->early.vec_ap.delay);

    /* The early reflection line. */
    length = EARLY_LINE_LENGTHS[3] * multiplier;
    total_samples += CalcLineLength(length, total_samples, frequency, 0,
                                   &state->early.delay);

    /* The late vector all-pass line. */
    length = LATE_ALLPASS_LENGTHS[3] * multiplier;
    total_samples += CalcLineLength(length, total_samples, frequency, 0,
                                   &state->late.vec_ap.delay);

    /* The late delay lines are calculated from the larger of the maximum
     * density line length or the maximum echo time, and includes the maximum
     * modulation-related delay. The modulator's delay is calculated from the
     * maximum modulation time and depth coefficient, and halved for the low-
     * to-high frequency swing.
     */
    length = maxf(AL_EAXREVERB_MAX_ECHO_TIME, LATE_LINE_LENGTHS[3]*multiplier) +
             AL_EAXREVERB_MAX_MODULATION_TIME*MODULATION_DEPTH_COEFF/2.0f;
    total_samples += CalcLineLength(length, total_samples, frequency, 0,
                                   &state->late.delay);

    if(total_samples != state->total_samples)
    {
        state->sample_buffer.resize(4 * total_samples);
        state->total_samples = total_samples;
    }

    /* Update all delays to reflect the new sample buffer. */
    RealizeLineOffset(state->sample_buffer, &state->delay);
    RealizeLineOffset(state->sample_buffer, &state->early.vec_ap.delay);
    RealizeLineOffset(state->sample_buffer, &state->early.delay);
    RealizeLineOffset(state->sample_buffer, &state->late.vec_ap.delay);
    RealizeLineOffset(state->sample_buffer, &state->late.delay);

    /* Clear the sample buffer. */
    std::fill(state->sample_buffer.begin(), state->sample_buffer.end(), 0.0F);

    return AL_TRUE;
}


/**************************************
 *  Effect Update                     *
 **************************************/

/* Calculate a decay coefficient given the length of each cycle and the time
 * until the decay reaches -60 dB.
 */
static inline ALfloat CalcDecayCoeff(
    const ALfloat length,
    const ALfloat decayTime)
{
    return powf(REVERB_DECAY_GAIN, length/decayTime);
}

/* Calculate a decay length from a coefficient and the time until the decay
 * reaches -60 dB.
 */
static inline ALfloat CalcDecayLength(
    const ALfloat coeff,
    const ALfloat decay_time)
{
    return log10f(coeff) * decay_time / log10f(REVERB_DECAY_GAIN);
}

/* Calculate an attenuation to be applied to the input of any echo models to
 * compensate for modal density and decay time.
 */
static inline ALfloat CalcDensityGain(
    const ALfloat a)
{
    /* The energy of a signal can be obtained by finding the area under the
     * squared signal.  This takes the form of Sum(x_n^2), where x is the
     * amplitude for the sample n.
     *
     * Decaying feedback matches exponential decay of the form Sum(a^n),
     * where a is the attenuation coefficient, and n is the sample.  The area
     * under this decay curve can be calculated as:  1 / (1 - a).
     *
     * Modifying the above equation to find the area under the squared curve
     * (for energy) yields:  1 / (1 - a^2).  Input attenuation can then be
     * calculated by inverting the square root of this approximation,
     * yielding:  1 / sqrt(1 / (1 - a^2)), simplified to: sqrt(1 - a^2).
     */
    return sqrtf(1.0f - a*a);
}

/* Calculate the scattering matrix coefficients given a diffusion factor. */
static inline ALvoid CalcMatrixCoeffs(
    const ALfloat diffusion,
    ALfloat *x,
    ALfloat *y)
{
    ALfloat n, t;

    /* The matrix is of order 4, so n is sqrt(4 - 1). */
    n = sqrtf(3.0f);
    t = diffusion * atanf(n);

    /* Calculate the first mixing matrix coefficient. */
    *x = cosf(t);
    /* Calculate the second mixing matrix coefficient. */
    *y = sinf(t) / n;
}

/* Calculate the limited HF ratio for use with the late reverb low-pass
 * filters.
 */
static ALfloat CalcLimitedHfRatio(
    const ALfloat hf_ratio,
    const ALfloat air_absorption_gain_hf,
    const ALfloat decay_time)
{
    ALfloat limit_ratio;

    /* Find the attenuation due to air absorption in dB (converting delay
     * time to meters using the speed of sound).  Then reversing the decay
     * equation, solve for HF ratio.  The delay length is cancelled out of
     * the equation, so it can be calculated once for all lines.
     */
    limit_ratio = 1.0f / (CalcDecayLength(air_absorption_gain_hf, decay_time) *
                         SPEEDOFSOUNDMETRESPERSEC);
    /* Using the limit calculated above, apply the upper bound to the HF
     * ratio. Also need to limit the result to a minimum of 0.1, just like
     * the HF ratio parameter.
     */
    return clampf(limit_ratio, 0.1f, hf_ratio);
}

/* Calculates the first-order high-pass coefficients following the I3DL2
 * reference model.  This is the transfer function:
 *
 *                1 - z^-1
 *     H(z) = p ------------
 *               1 - p z^-1
 *
 * And this is the I3DL2 coefficient calculation given gain (g) and reference
 * angular frequency (w):
 *
 *                                    g
 *      p = ------------------------------------------------------
 *          g cos(w) + sqrt((cos(w) - 1) (g^2 cos(w) + g^2 - 2))
 *
 * The coefficient is applied to the partial differential filter equation as:
 *
 *     c_0 = p
 *     c_1 = -p
 *     c_2 = p
 *     y_i = c_0 x_i + c_1 x_(i-1) + c_2 y_(i-1)
 *
 */
static inline void CalcHighpassCoeffs(
    const ALfloat gain,
    const ALfloat w,
    ALfloat coeffs[3])
{
    ALfloat g, g2, cw, p;

    if(gain >= 1.0f)
    {
        coeffs[0] = 1.0f;
        coeffs[1] = 0.0f;
        coeffs[2] = 0.0f;

        return;
    }

    g = maxf(0.001f, gain);
    g2 = g * g;
    cw = cosf(w);
    p = g / (g*cw + sqrtf((cw - 1.0f) * (g2*cw + g2 - 2.0f)));

    coeffs[0] = p;
    coeffs[1] = -p;
    coeffs[2] = p;
}

/* Calculates the first-order low-pass coefficients following the I3DL2
 * reference model.  This is the transfer function:
 *
 *              (1 - a) z^0
 *     H(z) = ----------------
 *             1 z^0 - a z^-1
 *
 * And this is the I3DL2 coefficient calculation given gain (g) and reference
 * angular frequency (w):
 *
 *          1 - g^2 cos(w) - sqrt(2 g^2 (1 - cos(w)) - g^4 (1 - cos(w)^2))
 *     a = ----------------------------------------------------------------
 *                                    1 - g^2
 *
 * The coefficient is applied to the partial differential filter equation as:
 *
 *     c_0 = 1 - a
 *     c_1 = 0
 *     c_2 = a
 *     y_i = c_0 x_i + c_1 x_(i-1) + c_2 y_(i-1)
 *
 */
static inline void CalcLowpassCoeffs(
    const ALfloat gain,
    const ALfloat w,
    ALfloat coeffs[3])
{
    ALfloat g, g2, cw, a;

    if(gain >= 1.0f)
    {
        coeffs[0] = 1.0f;
        coeffs[1] = 0.0f;
        coeffs[2] = 0.0f;

        return;
    }

    /* Be careful with gains < 0.001, as that causes the coefficient
     * to head towards 1, which will flatten the signal. */
    g = maxf(0.001f, gain);
    g2 = g * g;
    cw = cosf(w);
    a = (1.0f - g2*cw - sqrtf((2.0f*g2*(1.0f - cw)) - g2*g2*(1.0f - cw*cw))) /
        (1.0f - g2);

    coeffs[0] = 1.0f - a;
    coeffs[1] = 0.0f;
    coeffs[2] = a;
}

/* Calculates the first-order low-shelf coefficients.  The shelf filters are
 * used in place of low/high-pass filters to preserve the mid-band.  This is
 * the transfer function:
 *
 *             a_0 + a_1 z^-1
 *     H(z) = ----------------
 *              1 + b_1 z^-1
 *
 * And these are the coefficient calculations given cut gain (g) and a center
 * angular frequency (w):
 *
 *          sin(0.5 (pi - w) - 0.25 pi)
 *     p = -----------------------------
 *          sin(0.5 (pi - w) + 0.25 pi)
 *
 *          g + 1           g + 1
 *     a = ------- + sqrt((-------)^2 - 1)
 *          g - 1           g - 1
 *
 *            1 + g + (1 - g) a
 *     b_0 = -------------------
 *                    2
 *
 *            1 - g + (1 + g) a
 *     b_1 = -------------------
 *                    2
 *
 * The coefficients are applied to the partial differential filter equation
 * as:
 *
 *            b_0 + p b_1
 *     c_0 = -------------
 *              1 + p a
 *
 *            -(b_1 + p b_0)
 *     c_1 = ----------------
 *               1 + p a
 *
 *             p + a
 *     c_2 = ---------
 *            1 + p a
 *
 *     y_i = c_0 x_i + c_1 x_(i-1) + c_2 y_(i-1)
 *
 */
static inline void CalcLowShelfCoeffs(
    const ALfloat gain,
    const ALfloat w,
    ALfloat coeffs[3])
{
    ALfloat g, rw, p, n;
    ALfloat alpha, beta0, beta1;

    if(gain >= 1.0f)
    {
        coeffs[0] = 1.0f;
        coeffs[1] = 0.0f;
        coeffs[2] = 0.0f;

        return;
    }

    g = maxf(0.001f, gain);
    rw = F_PI - w;
    p = sinf(0.5f*rw - 0.25f*F_PI) / sinf(0.5f*rw + 0.25f*F_PI);
    n = (g + 1.0f) / (g - 1.0f);
    alpha = n + sqrtf(n*n - 1.0f);
    beta0 = (1.0f + g + (1.0f - g)*alpha) / 2.0f;
    beta1 = (1.0f - g + (1.0f + g)*alpha) / 2.0f;

    coeffs[0] = (beta0 + p*beta1) / (1.0f + p*alpha);
    coeffs[1] = -(beta1 + p*beta0) / (1.0f + p*alpha);
    coeffs[2] = (p + alpha) / (1.0f + p*alpha);
}

/* Calculates the first-order high-shelf coefficients.  The shelf filters are
 * used in place of low/high-pass filters to preserve the mid-band.  This is
 * the transfer function:
 *
 *             a_0 + a_1 z^-1
 *     H(z) = ----------------
 *              1 + b_1 z^-1
 *
 * And these are the coefficient calculations given cut gain (g) and a center
 * angular frequency (w):
 *
 *          sin(0.5 w - 0.25 pi)
 *     p = ----------------------
 *          sin(0.5 w + 0.25 pi)
 *
 *          g + 1           g + 1
 *     a = ------- + sqrt((-------)^2 - 1)
 *          g - 1           g - 1
 *
 *            1 + g + (1 - g) a
 *     b_0 = -------------------
 *                    2
 *
 *            1 - g + (1 + g) a
 *     b_1 = -------------------
 *                    2
 *
 * The coefficients are applied to the partial differential filter equation
 * as:
 *
 *            b_0 + p b_1
 *     c_0 = -------------
 *              1 + p a
 *
 *            b_1 + p b_0
 *     c_1 = -------------
 *              1 + p a
 *
 *            -(p + a)
 *     c_2 = ----------
 *            1 + p a
 *
 *     y_i = c_0 x_i + c_1 x_(i-1) + c_2 y_(i-1)
 *
 */
static inline void CalcHighShelfCoeffs(
    const ALfloat gain,
    const ALfloat w,
    ALfloat coeffs[3])
{
    ALfloat g, p, n;
    ALfloat alpha, beta0, beta1;

    if(gain >= 1.0f)
    {
        coeffs[0] = 1.0f;
        coeffs[1] = 0.0f;
        coeffs[2] = 0.0f;

        return;
    }

    g = maxf(0.001f, gain);
    p = sinf(0.5f*w - 0.25f*F_PI) / sinf(0.5f*w + 0.25f*F_PI);
    n = (g + 1.0f) / (g - 1.0f);
    alpha = n + sqrtf(n*n - 1.0f);
    beta0 = (1.0f + g + (1.0f - g)*alpha) / 2.0f;
    beta1 = (1.0f - g + (1.0f + g)*alpha) / 2.0f;

    coeffs[0] = (beta0 + p*beta1) / (1.0f + p*alpha);
    coeffs[1] = (beta1 + p*beta0) / (1.0f + p*alpha);
    coeffs[2] = -(p + alpha) / (1.0f + p*alpha);
}

/* Calculates the 3-band T60 damping coefficients for a particular delay line
 * of specified length using a combination of two low/high-pass/shelf or
 * pass-through filter sections (producing 3 coefficients each) and a general
 * gain (7th coefficient) given decay times for each band split at two (LF/
 * HF) reference frequencies (w).
 */
static void CalcT60DampingCoeffs(
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
    ALfloat lf_gain = CalcDecayCoeff(length, lf_decay_time);
    ALfloat mf_gain = CalcDecayCoeff(length, mf_decay_time);
    ALfloat hf_gain = CalcDecayCoeff(length, hf_decay_time);

    if(lf_gain < mf_gain)
    {
        if(mf_gain < hf_gain)
        {
            CalcLowShelfCoeffs(mf_gain / hf_gain, hf_w, lfcoeffs);
            CalcHighpassCoeffs(lf_gain / mf_gain, lf_w, hfcoeffs);
            *midcoeff = hf_gain;
        }
        else if(mf_gain > hf_gain)
        {
            CalcHighpassCoeffs(lf_gain / mf_gain, lf_w, lfcoeffs);
            CalcLowpassCoeffs(hf_gain / mf_gain, hf_w, hfcoeffs);
            *midcoeff = mf_gain;
        }
        else
        {
            lfcoeffs[0] = 1.0f;
            lfcoeffs[1] = 0.0f;
            lfcoeffs[2] = 0.0f;
            CalcHighpassCoeffs(lf_gain / mf_gain, lf_w, hfcoeffs);
            *midcoeff = mf_gain;
        }
    }
    else if(lf_gain > mf_gain)
    {
        if(mf_gain < hf_gain)
        {
            ALfloat hg = mf_gain / lf_gain;
            ALfloat lg = mf_gain / hf_gain;

            CalcHighShelfCoeffs(hg, lf_w, lfcoeffs);
            CalcLowShelfCoeffs(lg, hf_w, hfcoeffs);
            *midcoeff = maxf(lf_gain, hf_gain) / maxf(hg, lg);
        }
        else if(mf_gain > hf_gain)
        {
            CalcHighShelfCoeffs(mf_gain / lf_gain, lf_w, lfcoeffs);
            CalcLowpassCoeffs(hf_gain / mf_gain, hf_w, hfcoeffs);
            *midcoeff = lf_gain;
        }
        else
        {
            lfcoeffs[0] = 1.0f;
            lfcoeffs[1] = 0.0f;
            lfcoeffs[2] = 0.0f;
            CalcHighShelfCoeffs(mf_gain / lf_gain, lf_w, hfcoeffs);
            *midcoeff = lf_gain;
        }
    }
    else
    {
        lfcoeffs[0] = 1.0f;
        lfcoeffs[1] = 0.0f;
        lfcoeffs[2] = 0.0f;

        if(mf_gain < hf_gain)
        {
            CalcLowShelfCoeffs(mf_gain / hf_gain, hf_w, hfcoeffs);
            *midcoeff = hf_gain;
        }
        else if(mf_gain > hf_gain)
        {
            CalcLowpassCoeffs(hf_gain / mf_gain, hf_w, hfcoeffs);
            *midcoeff = mf_gain;
        }
        else
        {
            hfcoeffs[0] = 1.0f;
            hfcoeffs[1] = 0.0f;
            hfcoeffs[2] = 0.0f;
            *midcoeff = mf_gain;
        }
    }
}

/* Update the EAX modulation index, range, and depth.  Keep in mind that this
 * kind of vibrato is additive and not multiplicative as one may expect.  The
 * downswing will sound stronger than the upswing.
 */
static ALvoid UpdateModulator(
    const ALfloat mod_time,
    const ALfloat mod_depth,
    const ALuint frequency,
    ReverbEffect *state)
{
    ALuint range;

    /* Modulation is calculated in two parts.
     *
     * The modulation time effects the speed of the sinus. An index out of the
     * current range (both in samples) is incremented each sample, so a longer
     * time implies a larger range. The range is bound to a reasonable minimum
     * (1 sample) and when the timing changes, the index is rescaled to the new
     * range to keep the sinus consistent.
     */
    range = maxi(fastf2i(mod_time*frequency), 1);
    state->mod.index = (ALuint)(state->mod.index * (uint64_t)range /
                                state->mod.range);
    state->mod.range = range;

    /* The modulation depth effects the scale of the sinus, which changes how
     * much extra delay is added to the delay line. This delay changing over
     * time changes the pitch, creating the modulation effect. The scale needs
     * to be multiplied by the modulation time so that a given depth produces a
     * consistent shift in frequency over all ranges of time. Since the depth
     * is applied to a sinus value, it needs to be halved for the sinus swing
     * in time (half of it is spent decreasing the frequency, half is spent
     * increasing it).
     */
    state->mod.depth = mod_depth * MODULATION_DEPTH_COEFF * mod_time / 2.0f *
                       frequency;
}

/* Update the offsets for the main effect delay line. */
static ALvoid UpdateDelayLine(
    const ALfloat early_delay,
    const ALfloat late_delay,
    const ALfloat density,
    const ALfloat decay_time,
    const ALuint frequency,
    ReverbEffect *state)
{
    ALfloat multiplier, length;
    ALuint i;

    multiplier = 1.0f + density*LINE_MULTIPLIER;

    /* Early reflection taps are decorrelated by means of an average room
     * reflection approximation described above the definition of the taps.
     * This approximation is linear and so the above density multiplier can
     * be applied to adjust the width of the taps.  A single-band decay
     * coefficient is applied to simulate initial attenuation and absorption.
     *
     * Late reverb taps are based on the late line lengths to allow a zero-
     * delay path and offsets that would continue the propagation naturally
     * into the late lines.
     */
    for(i = 0;i < 4;i++)
    {
        length = early_delay + EARLY_TAP_LENGTHS[i]*multiplier;
        state->early_delay_taps[i][1] = fastf2i(length * frequency);

        length = EARLY_TAP_LENGTHS[i]*multiplier;
        state->early_delay_coeffs[i] = CalcDecayCoeff(length, decay_time);

        length = late_delay + (LATE_LINE_LENGTHS[i] - LATE_LINE_LENGTHS[0])*0.25f*multiplier;
        state->late_delay_taps[i][1] = state->late_feed_tap + fastf2i(length * frequency);
    }
}

/* Update the early reflection line lengths and gain coefficients. */
static ALvoid UpdateEarlyLines(
    const ALfloat density,
    const ALfloat decay_time,
    const ALuint frequency,
    ReverbEffect *state)
{
    ALfloat multiplier, length;
    ALsizei i;

    multiplier = 1.0f + density*LINE_MULTIPLIER;

    for(i = 0;i < 4;i++)
    {
        /* Calculate the length (in seconds) of each all-pass line. */
        length = EARLY_ALLPASS_LENGTHS[i] * multiplier;

        /* Calculate the delay offset for each all-pass line. */
        state->early.vec_ap.offsets[i][1] = fastf2i(length * frequency);

        /* Calculate the length (in seconds) of each delay line. */
        length = EARLY_LINE_LENGTHS[i] * multiplier;

        /* Calculate the delay offset for each delay line. */
        state->early.offsets[i][1] = fastf2i(length * frequency);

        /* Calculate the gain (coefficient) for each line. */
        state->early.coeffs[i] = CalcDecayCoeff(length, decay_time);
    }
}

/* Update the late reverb line lengths and T60 coefficients. */
static ALvoid UpdateLateLines(
    const ALfloat density,
    const ALfloat diffusion,
    const ALfloat lf_decay_time,
    const ALfloat mf_decay_time,
    const ALfloat hf_decay_time,
    const ALfloat lf_w,
    const ALfloat hf_w,
    const ALfloat echo_time,
    const ALfloat echo_depth,
    const ALuint frequency,
    ReverbEffect *state)
{
    ALfloat multiplier, length, band_weights[3];
    ALsizei i;

    /* To compensate for changes in modal density and decay time of the late
     * reverb signal, the input is attenuated based on the maximal energy of
     * the outgoing signal.  This approximation is used to keep the apparent
     * energy of the signal equal for all ranges of density and decay time.
     *
     * The average length of the delay lines is used to calculate the
     * attenuation coefficient.
     */
    multiplier = 1.0f + density*LINE_MULTIPLIER;
    length = (LATE_LINE_LENGTHS[0] + LATE_LINE_LENGTHS[1] +
              LATE_LINE_LENGTHS[2] + LATE_LINE_LENGTHS[3]) / 4.0f * multiplier;
    /* Include the echo transformation (see below). */
    length = lerp(length, echo_time, echo_depth);
    length += (LATE_ALLPASS_LENGTHS[0] + LATE_ALLPASS_LENGTHS[1] +
               LATE_ALLPASS_LENGTHS[2] + LATE_ALLPASS_LENGTHS[3]) / 4.0f * multiplier;
    /* The density gain calculation uses an average decay time weighted by
     * approximate bandwidth.  This attempts to compensate for losses of
     * energy that reduce decay time due to scattering into highly attenuated
     * bands.
     */
    band_weights[0] = lf_w;
    band_weights[1] = hf_w - lf_w;
    band_weights[2] = F_TAU - hf_w;
    state->late.density_gain = CalcDensityGain(
        CalcDecayCoeff(length, (band_weights[0]*lf_decay_time + band_weights[1]*mf_decay_time +
                                band_weights[2]*hf_decay_time) / F_TAU)
    );

    for(i = 0;i < 4;i++)
    {
        /* Calculate the length (in seconds) of each all-pass line. */
        length = LATE_ALLPASS_LENGTHS[i] * multiplier;

        /* Calculate the delay offset for each all-pass line. */
        state->late.vec_ap.offsets[i][1] = fastf2i(length * frequency);

        /* Calculate the length (in seconds) of each delay line.  This also
         * applies the echo transformation.  As the EAX echo depth approaches
         * 1, the line lengths approach a length equal to the echoTime.  This
         * helps to produce distinct echoes along the tail.
         */
        length = lerp(LATE_LINE_LENGTHS[i] * multiplier, echo_time, echo_depth);

        /* Calculate the delay offset for each delay line. */
        state->late.offsets[i][1] = fastf2i(length * frequency);

        /* Approximate the absorption that the vector all-pass would exhibit
         * given the current diffusion so we don't have to process a full T60
         * filter for each of its four lines.
         */
        length += lerp(LATE_ALLPASS_LENGTHS[i],
                       (LATE_ALLPASS_LENGTHS[0] + LATE_ALLPASS_LENGTHS[1] +
                        LATE_ALLPASS_LENGTHS[2] + LATE_ALLPASS_LENGTHS[3]) / 4.0f,
                       diffusion) * multiplier;

        /* Calculate the T60 damping coefficients for each line. */
        CalcT60DampingCoeffs(length, lf_decay_time, mf_decay_time, hf_decay_time,
                             lf_w, hf_w, state->late.filters[i].lf_coeffs,
                             state->late.filters[i].hf_coeffs,
                             &state->late.filters[i].mid_coeff);
    }
}

static void MATRIX_MULT(
    aluMatrixf& res,
    const aluMatrixf& m1,
    const aluMatrixf& m2)
{
    for(int col = 0;col < 4; ++col)
    {
        for(int row = 0;row < 4; ++row)
        {
            res.m[row][col] =
                m1.m[row][0]*m2.m[0][col] +
                m1.m[row][1]*m2.m[1][col] +
                m1.m[row][2]*m2.m[2][col] +
                m1.m[row][3]*m2.m[3][col];
        }
    }
}

/* Creates a transform matrix given a reverb vector. This works by creating a
 * Z-focus transform, then a rotate transform around X, then Y, to place the
 * focal point in the direction of the vector, using the vector length as a
 * focus strength.
 *
 * This isn't technically correct since the vector is supposed to define the
 * aperture and not rotate the perceived soundfield, but in practice it's
 * probably good enough.
 */
static aluMatrixf GetTransformFromVector(
    const ALfloat *vec)
{
    aluMatrixf zfocus, xrot, yrot;
    aluMatrixf tmp1, tmp2;
    ALfloat length;
    ALfloat sa, a;

    length = sqrtf(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);

    /* Define a Z-focus (X in Ambisonics) transform, given the panning vector
     * length.
     */
    sa = sinf(minf(length, 1.0f) * (F_PI/4.0f));
    aluMatrixfSet(&zfocus,
                     1.0f/(1.0f+sa),                       0.0f,                       0.0f, (sa/(1.0f+sa))/1.732050808f,
                               0.0f, sqrtf((1.0f-sa)/(1.0f+sa)),                       0.0f,                        0.0f,
                               0.0f,                       0.0f, sqrtf((1.0f-sa)/(1.0f+sa)),                        0.0f,
        (sa/(1.0f+sa))*1.732050808f,                       0.0f,                       0.0f,              1.0f/(1.0f+sa)
    );

    /* Define rotation around X (Y in Ambisonics) */
    a = atan2f(vec[1], sqrtf(vec[0]*vec[0] + vec[2]*vec[2]));
    aluMatrixfSet(&xrot,
        1.0f, 0.0f,     0.0f,    0.0f,
        0.0f, 1.0f,     0.0f,    0.0f,
        0.0f, 0.0f,  cosf(a), sinf(a),
        0.0f, 0.0f, -sinf(a), cosf(a)
    );

    /* Define rotation around Y (Z in Ambisonics). NOTE: EFX's reverb vectors
     * use a right-handled coordinate system, compared to the rest of OpenAL
     * which uses left-handed. This is fixed by negating Z, however it would
     * need to also be negated to get a proper Ambisonics angle, thus
     * cancelling it out.
     */
    a = atan2f(-vec[0], vec[2]);
    aluMatrixfSet(&yrot,
        1.0f,     0.0f, 0.0f,    0.0f,
        0.0f,  cosf(a), 0.0f, sinf(a),
        0.0f,     0.0f, 1.0f,    0.0f,
        0.0f, -sinf(a), 0.0f, cosf(a)
    );

    /* Define a matrix that first focuses on Z, then rotates around X then Y to
     * focus the output in the direction of the vector.
     */
    MATRIX_MULT(tmp1, xrot, zfocus);
    MATRIX_MULT(tmp2, yrot, tmp1);

    return tmp2;
}

/* Note: _res is transposed. */
static void MATRIX_MULT_T(
    aluMatrixf& res,
    const aluMatrixf& m1,
    const aluMatrixf& m2)
{
    for(int col = 0;col < 4; ++col)
    {
        for(int row = 0;row < 4; ++row)
        {
            res.m[col][row] =
                m1.m[row][0]*m2.m[0][col] +
                m1.m[row][1]*m2.m[1][col] +
                m1.m[row][2]*m2.m[2][col] +
                m1.m[row][3]*m2.m[3][col];
        }
    }
}

/* Update the early and late 3D panning gains. */
static ALvoid Update3DPanning(
    const ALCdevice *device,
    const ALfloat *reflections_pan,
    const ALfloat *late_reverb_pan,
    const ALfloat gain,
    const ALfloat early_gain,
    const ALfloat late_gain,
    ReverbEffect *state)
{
    aluMatrixf transform, rot;
    ALsizei i;

    state->out_buffer = device->foa_out.buffer;
    state->out_channels = device->foa_out.num_channels;

    /* Create a matrix that first converts A-Format to B-Format, then rotates
     * the B-Format soundfield according to the panning vector.
     */
    rot = GetTransformFromVector(reflections_pan);
    MATRIX_MULT_T(transform, rot, A2B);
    memset(&state->early.pan_gains, 0, sizeof(state->early.pan_gains));
    for(i = 0;i < MAX_EFFECT_CHANNELS;i++)
        ComputeFirstOrderGains(device->foa_out, transform.m[i], gain*early_gain, state->early.pan_gains[i]);

    rot = GetTransformFromVector(late_reverb_pan);
    MATRIX_MULT_T(transform, rot, A2B);
    memset(&state->late.pan_gains, 0, sizeof(state->late.pan_gains));
    for(i = 0;i < MAX_EFFECT_CHANNELS;i++)
        ComputeFirstOrderGains(device->foa_out, transform.m[i], gain*late_gain, state->late.pan_gains[i]);
}


/**************************************
 *  Effect Processing                 *
 **************************************/

/* Basic delay line input/output routines. */
static inline ALfloat DelayLineOut(
    const DelayLineI *delay,
    const ALsizei offset,
    const ALsizei c)
{
    return delay->lines[offset&delay->mask][c];
}

/* Cross-faded delay line output routine.  Instead of interpolating the
 * offsets, this interpolates (cross-fades) the outputs at each offset.
 */
static inline ALfloat FadedDelayLineOut(
    const DelayLineI *delay,
    const ALsizei off0,
    const ALsizei off1,
    const ALsizei c,
    const ALfloat mu)
{
    return lerp(delay->lines[off0&delay->mask][c], delay->lines[off1&delay->mask][c], mu);
}

static ALfloat DELAY_OUT_Faded(
    const DelayLineI* delay,
    const ALsizei off0,
    const ALsizei off1,
    const ALsizei c,
    const ALfloat mu)
{
    return FadedDelayLineOut(delay, off0, off1, c, mu);
}

static ALfloat DELAY_OUT_Unfaded(
    const DelayLineI* delay,
    const ALsizei off0,
    const ALsizei off1,
    const ALsizei c,
    const ALfloat mu)
{
    static_cast<void>(off1);
    static_cast<void>(mu);

    return DelayLineOut(delay, off0, c);
}

using DelayOutFunc = ALfloat (*)(
    const DelayLineI* delay,
    const ALsizei off0,
    const ALsizei off1,
    const ALsizei c,
    const ALfloat mu);

static inline ALvoid DelayLineIn(
    DelayLineI *delay,
    const ALsizei offset,
    const ALsizei c,
    const ALfloat in)
{
    delay->lines[offset&delay->mask][c] = in;
}

static inline ALvoid DelayLineIn4(
    DelayLineI *delay,
    ALsizei offset,
    const ALfloat in[4])
{
    ALsizei i;
    offset &= delay->mask;
    for(i = 0;i < 4;i++)
        delay->lines[offset][i] = in[i];
}

static inline ALvoid DelayLineIn4Rev(
    DelayLineI *delay,
    ALsizei offset,
    const ALfloat in[4])
{
    ALsizei i;
    offset &= delay->mask;
    for(i = 0;i < 4;i++)
        delay->lines[offset][i] = in[3-i];
}

static void CalcModulationDelays(
    ReverbEffect *state,
    ALint *delays,
    const ALsizei todo)
{
    ALfloat sinus, range;
    ALsizei index, i;

    index = state->mod.index;
    range = state->mod.filter;
    for(i = 0;i < todo;i++)
    {
        /* Calculate the sinus rhythm (dependent on modulation time and the
         * sampling rate).
         */
        sinus = sinf(F_TAU * index / state->mod.range);

        /* Step the modulation index forward, keeping it bound to its range. */
        index = (index+1) % state->mod.range;

        /* The depth determines the range over which to read the input samples
         * from, so it must be filtered to reduce the distortion caused by even
         * small parameter changes.
         */
        range = lerp(range, state->mod.depth, state->mod.coeff);

        /* Calculate the read offset. */
        delays[i] = lroundf(range*sinus);
    }
    state->mod.index = index;
    state->mod.filter = range;
}

/* Applies a scattering matrix to the 4-line (vector) input.  This is used
 * for both the below vector all-pass model and to perform modal feed-back
 * delay network (FDN) mixing.
 *
 * The matrix is derived from a skew-symmetric matrix to form a 4D rotation
 * matrix with a single unitary rotational parameter:
 *
 *     [  d,  a,  b,  c ]          1 = a^2 + b^2 + c^2 + d^2
 *     [ -a,  d,  c, -b ]
 *     [ -b, -c,  d,  a ]
 *     [ -c,  b, -a,  d ]
 *
 * The rotation is constructed from the effect's diffusion parameter,
 * yielding:
 *
 *     1 = x^2 + 3 y^2
 *
 * Where a, b, and c are the coefficient y with differing signs, and d is the
 * coefficient x.  The final matrix is thus:
 *
 *     [  x,  y, -y,  y ]          n = sqrt(matrix_order - 1)
 *     [ -y,  x,  y,  y ]          t = diffusion_parameter * atan(n)
 *     [  y, -y,  x,  y ]          x = cos(t)
 *     [ -y, -y, -y,  x ]          y = sin(t) / n
 *
 * Any square orthogonal matrix with an order that is a power of two will
 * work (where ^T is transpose, ^-1 is inverse):
 *
 *     M^T = M^-1
 *
 * Using that knowledge, finding an appropriate matrix can be accomplished
 * naively by searching all combinations of:
 *
 *     M = D + S - S^T
 *
 * Where D is a diagonal matrix (of x), and S is a triangular matrix (of y)
 * whose combination of signs are being iterated.
 */
static inline void VectorPartialScatter(
    ALfloat *vec,
    const ALfloat x_coeff,
    const ALfloat y_coeff)
{
    const ALfloat f[4] = { vec[0], vec[1], vec[2], vec[3] };

    vec[0] = x_coeff*f[0] + y_coeff*(         f[1] + -f[2] +  f[3]);
    vec[1] = x_coeff*f[1] + y_coeff*(-f[0]         +  f[2] +  f[3]);
    vec[2] = x_coeff*f[2] + y_coeff*( f[0] + -f[1]         +  f[3]);
    vec[3] = x_coeff*f[3] + y_coeff*(-f[0] + -f[1] + -f[2]        );
}

/* This applies a Gerzon multiple-in/multiple-out (MIMO) vector all-pass
 * filter to the 4-line input.
 *
 * It works by vectorizing a regular all-pass filter and replacing the delay
 * element with a scattering matrix (like the one above) and a diagonal
 * matrix of delay elements.
 *
 * Two static specializations are used for transitional (cross-faded) delay
 * line processing and non-transitional processing.
 */
static void VectorAllpassT(
    DelayOutFunc delay_out_func,
    ALfloat *vec,
    const ALsizei offset,
    const ALfloat feed_coeff,
    const ALfloat x_coeff,
    const ALfloat y_coeff,
    const ALfloat mu,
    VecAllpass *vap)
{
    ALfloat f[4];

    for(int i = 0;i < 4;i++)
    {
        auto input = vec[i];

        vec[i] = delay_out_func(
            &vap->delay,
            offset-vap->offsets[i][0],
            offset-vap->offsets[i][1],
            i,
            mu) - feed_coeff*input;

        f[i] = input + (feed_coeff * vec[i]);
    }

    VectorPartialScatter(f, x_coeff, y_coeff);

    DelayLineIn4(&vap->delay, offset, f);
}

static void VectorAllpass_Unfaded(
    ALfloat *vec,
    const ALsizei offset,
    const ALfloat feed_coeff,
    const ALfloat x_coeff,
    const ALfloat y_coeff,
    const ALfloat mu,
    VecAllpass *vap)
{
    VectorAllpassT(DELAY_OUT_Unfaded, vec, offset, feed_coeff, x_coeff, y_coeff, mu, vap);
}

static void VectorAllpass_Faded(
    ALfloat *vec,
    const ALsizei offset,
    const ALfloat feed_coeff,
    const ALfloat x_coeff,
    const ALfloat y_coeff,
    const ALfloat mu,
    VecAllpass *vap)
{
    VectorAllpassT(DELAY_OUT_Faded, vec, offset, feed_coeff, x_coeff, y_coeff, mu, vap);
}

/* A helper to reverse vector components. */
static inline void VectorReverse(
    ALfloat vec[4])
{
    const ALfloat f[4] = { vec[0], vec[1], vec[2], vec[3] };

    vec[0] = f[3];
    vec[1] = f[2];
    vec[2] = f[1];
    vec[3] = f[0];
}


using VectorAllpassFunc = void (*)(
    ALfloat *vec,
    const ALsizei offset,
    const ALfloat feed_coeff,
    const ALfloat x_coeff,
    const ALfloat y_coeff,
    const ALfloat mu,
    VecAllpass *vap);

/* This generates early reflections.
 *
 * This is done by obtaining the primary reflections (those arriving from the
 * same direction as the source) from the main delay line.  These are
 * attenuated and all-pass filtered (based on the diffusion parameter).
 *
 * The early lines are then fed in reverse (according to the approximately
 * opposite spatial location of the A-Format lines) to create the secondary
 * reflections (those arriving from the opposite direction as the source).
 *
 * The early response is then completed by combining the primary reflections
 * with the delayed and attenuated output from the early lines.
 *
 * Finally, the early response is reversed, scattered (based on diffusion),
 * and fed into the late reverb section of the main delay line.
 *
 * Two static specializations are used for transitional (cross-faded) delay
 * line processing and non-transitional processing.
 */
static ALvoid EarlyReflectionT(
    VectorAllpassFunc vector_allpass_func,
    DelayOutFunc delay_out_func,
    ReverbEffect* state,
    const ALsizei todo,
    ALfloat fade,
    ALfloat (*out)[MAX_UPDATE_SAMPLES])
{
    ALsizei current_offset = state->offset;
    const ALfloat ap_feed_coeff = state->ap_feed_coeff;
    const ALfloat mix_x = state->mix_x;
    const ALfloat mix_y = state->mix_y;
    ALfloat f[4];

    for (int i = 0; i < todo; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            f[j] = delay_out_func(
                &state->delay,
                current_offset - state->early_delay_taps[j][0],
                current_offset - state->early_delay_taps[j][1],
                j, fade) * state->early_delay_coeffs[j];
        }

        vector_allpass_func(f, current_offset, ap_feed_coeff, mix_x, mix_y, fade, &state->early.vec_ap);

        DelayLineIn4Rev(&state->early.delay, current_offset, f);

        for (int j = 0; j < 4; j++)
        {
            f[j] += delay_out_func(&state->early.delay,
                current_offset - state->early.offsets[j][0],
                current_offset - state->early.offsets[j][1], j, fade
            ) * state->early.coeffs[j];
        }

        for (int j = 0; j < 4; j++)
        {
            out[j][i] = f[j];
        }

        VectorReverse(f);

        VectorPartialScatter(f, mix_x, mix_y);

        DelayLineIn4(&state->delay, current_offset - state->late_feed_tap, f);

        current_offset += 1;
        fade += FadeStep;
    }
}

static void EarlyReflection_Unfaded(
    ReverbEffect* state,
    const ALsizei todo,
    ALfloat fade,
    ALfloat (*out)[MAX_UPDATE_SAMPLES])
{
    EarlyReflectionT(VectorAllpass_Unfaded, DELAY_OUT_Unfaded, state, todo, fade, out);
}

static void EarlyReflection_Faded(
    ReverbEffect* state,
    const ALsizei todo,
    ALfloat fade,
    ALfloat (*out)[MAX_UPDATE_SAMPLES])
{
    EarlyReflectionT(VectorAllpass_Faded, DELAY_OUT_Faded, state, todo, fade, out);
}

/* Applies a first order filter section. */
static inline ALfloat FirstOrderFilter(
    const ALfloat in,
    const ALfloat coeffs[3],
    ALfloat state[2])
{
    ALfloat out = coeffs[0]*in + coeffs[1]*state[0] + coeffs[2]*state[1];

    state[0] = in;
    state[1] = out;

    return out;
}

/* Applies the two T60 damping filter sections. */
static inline ALfloat LateT60Filter(
    const ALsizei index,
    const ALfloat in,
    ReverbEffect *state)
{
    ALfloat out = FirstOrderFilter(in, state->late.filters[index].lf_coeffs,
                                   state->late.filters[index].states[0]);

    return state->late.filters[index].mid_coeff *
           FirstOrderFilter(out, state->late.filters[index].hf_coeffs,
                            state->late.filters[index].states[1]);
}

/* This generates the reverb tail using a modified feed-back delay network
 * (FDN).
 *
 * Results from the early reflections are attenuated by the density gain and
 * mixed with the output from the late delay lines.
 *
 * The late response is then completed by T60 and all-pass filtering the mix.
 *
 * Finally, the lines are reversed (so they feed their opposite directions)
 * and scattered with the FDN matrix before re-feeding the delay lines.
 *
 * Two static specializations are used for transitional (cross-faded) delay
 * line processing and non-transitional processing.
 */
static ALvoid LateReverbT(
    VectorAllpassFunc vector_allpass_func,
    DelayOutFunc delay_out_func,
    ReverbEffect* state,
    const ALsizei todo,
    ALfloat fade,
    ALfloat (*out)[MAX_UPDATE_SAMPLES])
{
    const ALfloat ap_feed_coeff = state->ap_feed_coeff;
    const ALfloat mix_x = state->mix_x;
    const ALfloat mix_y = state->mix_y;
    ALint moddelay[MAX_UPDATE_SAMPLES];
    ALsizei current_delay;
    ALsizei current_offset;
    ALfloat f[4];

    CalcModulationDelays(state, moddelay, todo);

    current_offset = state->offset;
    for (int i = 0; i < todo; i++)
    {
        for (int j = 0; j < 4; j++)
            f[j] = delay_out_func(&state->delay,
                current_offset - state->late_delay_taps[j][0],
                current_offset - state->late_delay_taps[j][1], j, fade
            ) * state->late.density_gain;

        current_delay = current_offset - moddelay[i];
        for (int j = 0; j < 4; j++)
            f[j] += delay_out_func(&state->late.delay,
                current_delay - state->late.offsets[j][0],
                current_delay - state->late.offsets[j][1], j, fade
            );

        for (int j = 0; j < 4; j++)
            f[j] = LateT60Filter(j, f[j], state);

        vector_allpass_func(f, current_offset, ap_feed_coeff, mix_x, mix_y, fade,
            &state->late.vec_ap);

        for (int j = 0; j < 4; j++)
            out[j][i] = f[j];

        VectorReverse(f);

        VectorPartialScatter(f, mix_x, mix_y);

        DelayLineIn4(&state->late.delay, current_offset, f);

        current_offset++;
        fade += FadeStep;
    }
}

static void LateReverb_Unfaded(
    ReverbEffect* state,
    const ALsizei todo,
    ALfloat fade,
    ALfloat (*out)[MAX_UPDATE_SAMPLES])
{
    LateReverbT(VectorAllpass_Unfaded, DELAY_OUT_Unfaded, state, todo, fade, out);
}

static void LateReverb_Faded(
    ReverbEffect* state,
    const ALsizei todo,
    ALfloat fade,
    ALfloat (*out)[MAX_UPDATE_SAMPLES])
{
    LateReverbT(VectorAllpass_Faded, DELAY_OUT_Faded, state, todo, fade, out);
}

/* Perform the non-EAX reverb pass on a given input sample, resulting in
 * four-channel output.
 */
static ALfloat VerbPass(
    ReverbEffect *state,
    const ALsizei todo,
    ALfloat fade,
    const ALfloat (*input)[MAX_UPDATE_SAMPLES],
    ALfloat (*early)[MAX_UPDATE_SAMPLES],
    ALfloat (*late)[MAX_UPDATE_SAMPLES])
{
    ALsizei i, c;

    for(c = 0;c < 4;c++)
    {
        /* Low-pass filter the incoming samples (use the early buffer as temp
         * storage).
         */
        ALfilterState_processC(&state->filters[c].lp, &early[0][0], input[c], todo);

        /* Feed the initial delay line. */
        for(i = 0;i < todo;i++)
            DelayLineIn(&state->delay, state->offset+i, c, early[0][i]);
    }

    if(fade < 1.0f)
    {
        /* Generate early reflections. */
        EarlyReflection_Faded(state, todo, fade, early);

        /* Generate late reverb. */
        LateReverb_Faded(state, todo, fade, late);
        fade = minf(1.0f, fade + todo*FadeStep);
    }
    else
    {
        /* Generate early reflections. */
        EarlyReflection_Unfaded(state, todo, fade, early);

        /* Generate late reverb. */
        LateReverb_Unfaded(state, todo, fade, late);
    }

    /* Step all delays forward one sample. */
    state->offset += todo;

    return fade;
}

/* Perform the EAX reverb pass on a given input sample, resulting in four-
 * channel output.
 */
static ALfloat EAXVerbPass(
    ReverbEffect *state,
    const ALsizei todo,
    ALfloat fade,
    const ALfloat (*input)[MAX_UPDATE_SAMPLES],
    ALfloat (*early)[MAX_UPDATE_SAMPLES],
    ALfloat (*late)[MAX_UPDATE_SAMPLES])
{
    ALsizei i, c;

    for(c = 0;c < 4;c++)
    {
        /* Band-pass the incoming samples. Use the early output lines for temp
         * storage.
         */
        ALfilterState_processC(&state->filters[c].lp, early[0], input[c], todo);
        ALfilterState_processC(&state->filters[c].hp, early[1], early[0], todo);

        /* Feed the initial delay line. */
        for(i = 0;i < todo;i++)
            DelayLineIn(&state->delay, state->offset+i, c, early[1][i]);
    }

    if(fade < 1.0f)
    {
        /* Generate early reflections. */
        EarlyReflection_Faded(state, todo, fade, early);

        /* Generate late reverb. */
        LateReverb_Faded(state, todo, fade, late);
        fade = minf(1.0f, fade + todo*FadeStep);
    }
    else
    {
        /* Generate early reflections. */
        EarlyReflection_Unfaded(state, todo, fade, early);

        /* Generate late reverb. */
        LateReverb_Unfaded(state, todo, fade, late);
    }

    /* Step all delays forward. */
    state->offset += todo;

    return fade;
}


void ReverbEffect::do_construct()
{
    is_eax = AL_FALSE;

    total_samples = 0;
    sample_buffer = ReverbSampleBuffer{};

    for (int i = 0; i < 4; ++i)
    {
        ALfilterState_clear(&filters[i].lp);
        ALfilterState_clear(&filters[i].hp);
    }

    delay.mask = 0;
    delay.lines = nullptr;

    for (int i = 0; i < 4; ++i)
    {
        early_delay_taps[i][0] = 0;
        early_delay_taps[i][1] = 0;
        early_delay_coeffs[i] = 0.0f;
    }

    late_feed_tap = 0;

    for (int i = 0; i < 4; ++i)
    {
        late_delay_taps[i][0] = 0;
        late_delay_taps[i][1] = 0;
    }

    ap_feed_coeff = 0.0f;
    mix_x = 0.0f;
    mix_y = 0.0f;

    early.vec_ap.delay.mask = 0;
    early.vec_ap.delay.lines = nullptr;
    early.delay.mask = 0;
    early.delay.lines = nullptr;

    for (int i = 0; i < 4; i++)
    {
        early.vec_ap.offsets[i][0] = 0;
        early.vec_ap.offsets[i][1] = 0;
        early.offsets[i][0] = 0;
        early.offsets[i][1] = 0;
        early.coeffs[i] = 0.0f;
    }

    mod.index = 0;
    mod.range = 1;
    mod.depth = 0.0f;
    mod.coeff = 0.0f;
    mod.filter = 0.0f;

    late.density_gain = 0.0f;

    late.delay.mask = 0;
    late.delay.lines = nullptr;
    late.vec_ap.delay.mask = 0;
    late.vec_ap.delay.lines = nullptr;

    for (int i = 0; i < 4; ++i)
    {
        late.offsets[i][0] = 0;
        late.offsets[i][1] = 0;

        late.vec_ap.offsets[i][0] = 0;
        late.vec_ap.offsets[i][1] = 0;

        for (int j = 0; j < 3; ++j)
        {
            late.filters[i].lf_coeffs[j] = 0.0f;
            late.filters[i].hf_coeffs[j] = 0.0f;
        }

        late.filters[i].mid_coeff = 0.0f;

        late.filters[i].states[0][0] = 0.0f;
        late.filters[i].states[0][1] = 0.0f;
        late.filters[i].states[1][0] = 0.0f;
        late.filters[i].states[1][1] = 0.0f;
    }

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < MAX_OUTPUT_CHANNELS; ++j)
        {
            early.current_gains[i][j] = 0.0f;
            early.pan_gains[i][j] = 0.0f;
            late.current_gains[i][j] = 0.0f;
            late.pan_gains[i][j] = 0.0f;
        }
    }

    fade_count = 0;
    offset = 0;
}

void ReverbEffect::do_destruct()
{
    sample_buffer = ReverbSampleBuffer{};
}

ALboolean ReverbEffect::do_update_device(
    ALCdevice* device)
{
    ALuint frequency = device->frequency, i;
    ALfloat multiplier;

    /* Allocate the delay lines. */
    if (!AllocLines(frequency, this))
        return AL_FALSE;

    /* Calculate the modulation filter coefficient.  Notice that the exponent
    * is calculated given the current sample rate.  This ensures that the
    * resulting filter response over time is consistent across all sample
    * rates.
    */
    mod.coeff = powf(MODULATION_FILTER_COEFF,
        MODULATION_FILTER_CONST / frequency);

    multiplier = 1.0f + LINE_MULTIPLIER;

    /* The late feed taps are set a fixed position past the latest delay tap. */
    for (i = 0; i < 4; i++)
        late_feed_tap = fastf2i((AL_EAXREVERB_MAX_REFLECTIONS_DELAY +
            EARLY_TAP_LENGTHS[3] * multiplier) *
            frequency);

    return AL_TRUE;
}

void ReverbEffect::do_update(
    ALCdevice* device,
    const struct ALeffectslot* slot,
    const union ALeffectProps *props)
{
    ALuint frequency = device->frequency;
    ALfloat lf_scale, hf_scale, hf_ratio;
    ALfloat lf_decay_time, hf_decay_time;
    ALfloat gain, gainlf, gainhf;
    ALsizei i;

    if (slot->params.effect_type == AL_EFFECT_EAXREVERB)
        is_eax = AL_TRUE;
    else if (slot->params.effect_type == AL_EFFECT_REVERB)
        is_eax = AL_FALSE;

    /* Calculate the master filters */
    hf_scale = props->reverb.hf_reference / frequency;
    /* Restrict the filter gains from going below -60dB to keep the filter from
    * killing most of the signal.
    */
    gainhf = maxf(props->reverb.gain_hf, 0.001f);
    ALfilterState_setParams(&filters[0].lp, ALfilterType_HighShelf,
        gainhf, hf_scale, calc_rcpQ_from_slope(gainhf, 1.0f));
    lf_scale = props->reverb.lf_reference / frequency;
    gainlf = maxf(props->reverb.gain_lf, 0.001f);
    ALfilterState_setParams(&filters[0].hp, ALfilterType_LowShelf,
        gainlf, lf_scale, calc_rcpQ_from_slope(gainlf, 1.0f));
    for (i = 1; i < 4; i++)
    {
        ALfilterState_copyParams(&filters[i].lp, &filters[0].lp);
        ALfilterState_copyParams(&filters[i].hp, &filters[0].hp);
    }

    /* Update the main effect delay and associated taps. */
    UpdateDelayLine(props->reverb.reflections_delay, props->reverb.late_reverb_delay,
        props->reverb.density, props->reverb.decay_time, frequency,
        this);

    /* Calculate the all-pass feed-back/forward coefficient. */
    ap_feed_coeff = sqrtf(0.5f) * powf(props->reverb.diffusion, 2.0f);

    /* Update the early lines. */
    UpdateEarlyLines(props->reverb.density, props->reverb.decay_time,
        frequency, this);

    /* Get the mixing matrix coefficients. */
    CalcMatrixCoeffs(props->reverb.diffusion, &mix_x, &mix_y);

    /* If the HF limit parameter is flagged, calculate an appropriate limit
    * based on the air absorption parameter.
    */
    hf_ratio = props->reverb.decay_hf_ratio;
    if (props->reverb.decay_hf_limit && props->reverb.air_absorption_gain_hf < 1.0f)
        hf_ratio = CalcLimitedHfRatio(hf_ratio, props->reverb.air_absorption_gain_hf,
            props->reverb.decay_time);

    /* Calculate the LF/HF decay times. */
    lf_decay_time = clampf(props->reverb.decay_time * props->reverb.decay_lf_ratio,
        AL_EAXREVERB_MIN_DECAY_TIME, AL_EAXREVERB_MAX_DECAY_TIME);
    hf_decay_time = clampf(props->reverb.decay_time * hf_ratio,
        AL_EAXREVERB_MIN_DECAY_TIME, AL_EAXREVERB_MAX_DECAY_TIME);

    /* Update the modulator line. */
    UpdateModulator(props->reverb.modulation_time, props->reverb.modulation_depth,
        frequency, this);

    /* Update the late lines. */
    UpdateLateLines(props->reverb.density, props->reverb.diffusion,
        lf_decay_time, props->reverb.decay_time, hf_decay_time,
        F_TAU * lf_scale, F_TAU * hf_scale,
        props->reverb.echo_time, props->reverb.echo_depth,
        frequency, this);

    /* Update early and late 3D panning. */
    gain = props->reverb.gain;
    Update3DPanning(device, props->reverb.reflections_pan,
        props->reverb.late_reverb_pan, gain,
        props->reverb.reflections_gain,
        props->reverb.late_reverb_gain, this);

    /* Determine if delay-line cross-fading is required. */
    for (i = 0; i < 4; i++)
    {
        if ((early_delay_taps[i][1] != early_delay_taps[i][0]) ||
            (early.vec_ap.offsets[i][1] != early.vec_ap.offsets[i][0]) ||
            (early.offsets[i][1] != early.offsets[i][0]) ||
            (late_delay_taps[i][1] != late_delay_taps[i][0]) ||
            (late.vec_ap.offsets[i][1] != late.vec_ap.offsets[i][0]) ||
            (late.offsets[i][1] != late.offsets[i][0]))
        {
            fade_count = 0;
            break;
        }
    }
}

void ReverbEffect::do_process(
    const ALsizei sample_count,
    const SampleBuffers& src_samples,
    SampleBuffers& dst_samples,
    const ALsizei channel_count)
{
    auto reverb_proc = is_eax ? EAXVerbPass : VerbPass;
    auto afmt_samples = a_format_samples;
    auto late_samples = reverb_samples;
    ALfloat fade = (ALfloat)fade_count / FADE_SAMPLES;
    ALsizei base, c;

    /* Process reverb for these samples. */
    for (base = 0; base < sample_count;)
    {
        ALsizei todo = mini(sample_count - base, MAX_UPDATE_SAMPLES);
        /* If cross-fading, don't do more samples than there are to fade. */
        if (FADE_SAMPLES - fade_count > 0)
            todo = mini(todo, FADE_SAMPLES - fade_count);

        /* Convert B-Format to A-Format for processing. */
        memset(afmt_samples, 0, sizeof(*afmt_samples) * 4);
        for (c = 0; c < 4; c++)
            MixRowSamples(afmt_samples[c], B2A.m[c],
                src_samples, MAX_EFFECT_CHANNELS, base, todo
            );

        /* Process the samples for reverb. */
        fade = reverb_proc(this, todo, fade, afmt_samples, early_samples, late_samples);
        if (fade_count < FADE_SAMPLES && (fade_count += todo) >= FADE_SAMPLES)
        {
            /* Update the cross-fading delay line taps. */
            fade_count = FADE_SAMPLES;
            fade = 1.0f;
            for (c = 0; c < 4; c++)
            {
                early_delay_taps[c][0] = early_delay_taps[c][1];
                early.vec_ap.offsets[c][0] = early.vec_ap.offsets[c][1];
                early.offsets[c][0] = early.offsets[c][1];
                late_delay_taps[c][0] = late_delay_taps[c][1];
                late.vec_ap.offsets[c][0] = late.vec_ap.offsets[c][1];
                late.offsets[c][0] = late.offsets[c][1];
            }
        }

        /* Mix the A-Format results to output, implicitly converting back to
        * B-Format.
        */
        for (c = 0; c < 4; c++)
            MixSamples(early_samples[c], channel_count, dst_samples,
                early.current_gains[c], early.pan_gains[c],
                sample_count - base, base, todo
            );
        for (c = 0; c < 4; c++)
            MixSamples(late_samples[c], channel_count, dst_samples,
                late.current_gains[c], late.pan_gains[c],
                sample_count - base, base, todo
            );

        base += todo;
    }
}

IEffect* create_reverb_effect()
{
    return create_effect<ReverbEffect>();
}
