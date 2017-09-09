#ifndef _AL_FILTER_H_
#define _AL_FILTER_H_

#include "alMain.h"

#include "math_defs.h"


constexpr auto lp_frequency_reference = 5000.0F;
constexpr auto hp_frequency_reference = 250.0F;


// Filters implementation is based on the "Cookbook formulae for audio
// EQ biquad filter coefficients" by Robert Bristow-Johnson
// http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
//
// Implementation note: For the shelf filters, the specified gain is for the
// reference frequency, which is the centerpoint of the transition band. This
// better matches EFX filter design. To set the gain for the shelf itself, use
// the square root of the desired linear gain (or halve the dB gain).

enum class FilterType
{
    // EFX-style low-pass filter, specifying a gain and reference frequency.
    high_shelf,

    // EFX-style high-pass filter, specifying a gain and reference frequency.
    low_shelf,

    // Peaking filter, specifying a gain and reference frequency.
    peaking,

    // Low-pass cut-off filter, specifying a cut-off frequency.
    low_pass,

    // High-pass cut-off filter, specifying a cut-off frequency.
    high_pass,

    // Band-pass filter, specifying a center frequency.
    band_pass,
}; // FilterType

struct FilterState
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

    void reset();
}; // FilterState


// Calculates the rcpQ (i.e. 1/Q) coefficient for shelving filters, using the
// reference gain and shelf slope parameter.
// 0 < gain
// 0 < slope <= 1
float calc_rcp_q_from_slope(
    const float gain,
    const float slope);

// Calculates the rcpQ (i.e. 1/Q) coefficient for filters, using the frequency
// multiple (i.e. ref_freq / sampling_freq) and bandwidth.
// 0 < freq_mult < 0.5.
float calc_rcp_q_from_bandwidth(
    const float freq_mult,
    const float bandwidth);

void al_filter_state_clear(
    FilterState* filter);

void al_filter_state_set_params(
    FilterState* filter,
    const FilterType type,
    const float gain,
    const float freq_mult,
    const float rcp_q);

void al_filter_state_copy_params(
    FilterState* dst,
    const FilterState* src);

void al_filter_state_process_c(
    FilterState* filter,
    float* dst,
    const float* src,
    const int num_samples);

void al_filter_state_process_pass_through(
    FilterState* filter,
    const float* src,
    const int num_samples);


#endif
