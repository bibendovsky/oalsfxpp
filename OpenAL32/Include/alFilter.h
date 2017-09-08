#ifndef _AL_FILTER_H_
#define _AL_FILTER_H_

#include "alMain.h"

#include "math_defs.h"


constexpr auto LOWPASSFREQREF = 5000.0F;
constexpr auto HIGHPASSFREQREF = 250.0F;


/* Filters implementation is based on the "Cookbook formulae for audio
 * EQ biquad filter coefficients" by Robert Bristow-Johnson
 * http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
 */
/* Implementation note: For the shelf filters, the specified gain is for the
 * reference frequency, which is the centerpoint of the transition band. This
 * better matches EFX filter design. To set the gain for the shelf itself, use
 * the square root of the desired linear gain (or halve the dB gain).
 */

enum ALfilterType
{
    /** EFX-style low-pass filter, specifying a gain and reference frequency. */
    ALfilterType_HighShelf,
    /** EFX-style high-pass filter, specifying a gain and reference frequency. */
    ALfilterType_LowShelf,
    /** Peaking filter, specifying a gain and reference frequency. */
    ALfilterType_Peaking,

    /** Low-pass cut-off filter, specifying a cut-off frequency. */
    ALfilterType_LowPass,
    /** High-pass cut-off filter, specifying a cut-off frequency. */
    ALfilterType_HighPass,
    /** Band-pass filter, specifying a center frequency. */
    ALfilterType_BandPass,
}; // ALfilterType

struct ALfilterState
{
    float x[2]; // History of two last input samples
    float y[2]; // History of two last output samples

    // Transfer function coefficients "b"
    float b0;
    float b1;
    float b2;

    // Transfer function coefficients "a" (a0 is pre-applied)
    float a1;
    float a2;
}; // ALfilterState

/* Calculates the rcpQ (i.e. 1/Q) coefficient for shelving filters, using the
 * reference gain and shelf slope parameter.
 * 0 < gain
 * 0 < slope <= 1
 */
inline float calc_rcpQ_from_slope(float gain, float slope)
{
    return sqrtf((gain + 1.0f/gain)*(1.0f/slope - 1.0f) + 2.0f);
}
/* Calculates the rcpQ (i.e. 1/Q) coefficient for filters, using the frequency
 * multiple (i.e. ref_freq / sampling_freq) and bandwidth.
 * 0 < freq_mult < 0.5.
 */
inline float calc_rcpQ_from_bandwidth(float freq_mult, float bandwidth)
{
    float w0 = F_TAU * freq_mult;
    return 2.0f*std::sinh(std::log(2.0f)/2.0f*bandwidth*w0/std::sin(w0));
}

inline void ALfilterState_clear(ALfilterState *filter)
{
    filter->x[0] = 0.0f;
    filter->x[1] = 0.0f;
    filter->y[0] = 0.0f;
    filter->y[1] = 0.0f;
}

void ALfilterState_setParams(ALfilterState *filter, ALfilterType type, float gain, float freq_mult, float rcpQ);

inline void ALfilterState_copyParams(ALfilterState *dst, const ALfilterState *src)
{
    dst->b0 = src->b0;
    dst->b1 = src->b1;
    dst->b2 = src->b2;
    dst->a1 = src->a1;
    dst->a2 = src->a2;
}

void ALfilterState_processC(ALfilterState *filter, float *dst, const float *src, int numsamples);

inline void ALfilterState_processPassthru(ALfilterState *filter, const float *src, int numsamples)
{
    if(numsamples >= 2)
    {
        filter->x[1] = src[numsamples-2];
        filter->x[0] = src[numsamples-1];
        filter->y[1] = src[numsamples-2];
        filter->y[0] = src[numsamples-1];
    }
    else if(numsamples == 1)
    {
        filter->x[1] = filter->x[0];
        filter->x[0] = src[0];
        filter->y[1] = filter->y[0];
        filter->y[0] = src[0];
    }
}


#endif
