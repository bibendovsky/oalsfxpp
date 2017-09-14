#ifndef _ALU_H_
#define _ALU_H_


#include "alMain.h"
#include "alFilter.h"
#include "alAuxEffectSlot.h"


struct ALsource;
struct ALvoice;
struct EffectSlot;


struct Mat4F
{
    using Items = float[4][4];

    Items m_;


    float& operator()(
        const int row_index,
        const int column_index)
    {
        return m_[row_index][column_index];
    }

    const float& operator()(
        const int row_index,
        const int column_index) const
    {
        return m_[row_index][column_index];
    }
}; // Mat4F

constexpr Mat4F mat4f_identity = {{
    {1.0F, 0.0F, 0.0F, 0.0F,},
    {0.0F, 1.0F, 0.0F, 0.0F,},
    {0.0F, 0.0F, 1.0F, 0.0F,},
    {0.0F, 0.0F, 0.0F, 1.0F,},
}};

enum class ActiveFilters
{
    none = 0,
    low_pass = 1,
    high_pass = 2,
    band_pass = low_pass | high_pass
}; // ActiveFilters


using MixerFunc = void (*)(
    const float* data,
    const int channel_count,
    SampleBuffers& dst_buffers,
    float* current_gains,
    const float* target_gains,
    const int counter,
    const int dst_position,
    const int buffer_size);

using RowMixerFunc = void (*)(
    float* dst_buffer,
    const float* gains,
    const SampleBuffers& src_buffers,
    const int channel_count,
    const int src_position,
    const int buffer_size);


constexpr auto max_mix_gain = 16.0F; // +24dB

constexpr auto silence_threshold_gain = 0.00001F; // -100dB


template<typename T>
inline T clamp(
    const T value,
    const T min_value,
    const T max_value)
{
    return std::min(max_value, std::max(min_value, value));
}

float lerp(
    const float val1,
    const float val2,
    const float mu);


// Set up the appropriate panning method and mixing method given the device
// properties.
void alu_init_renderer(
    ALCdevice* device);

// Calculates ambisonic coefficients based on a direction vector. The vector
// must be normalized (unit length), and the spread is the angular width of the
// sound (0...tau).
void calc_direction_coeffs(
    const float dir[3],
    const float spread,
    float coeffs[max_ambi_coeffs]);

// Calculates ambisonic coefficients based on azimuth and elevation. The
// azimuth and elevation parameters are in radians, going right and up
// respectively.
void calc_angle_coeffs(
    const float azimuth,
    const float elevation,
    const float spread,
    float coeffs[max_ambi_coeffs]);

// Computes channel gains for ambient, omni-directional sounds.
void compute_ambient_gains(
    const ALCdevice* device,
    const float in_gain,
    float* const out_gains);

void compute_ambient_gains_mc(
    const ChannelConfig* channel_coeffs,
    const int num_channels,
    const float in_gain,
    float out_gains[max_channels]);

void compute_ambient_gains_bf(
    const int num_channels,
    const float in_gain,
    float gains[max_channels]);

// Computes panning gains using the given channel decoder coefficients and the
// pre-calculated direction or angle coefficients.
void compute_panning_gains(
    const ALCdevice* device,
    const float* const coeffs,
    const float in_gain,
    float* const out_gains);

void compute_panning_gains_mc(
    const ChannelConfig* chan_coeffs,
    const int num_chans,
    const int num_coeffs,
    const float coeffs[max_ambi_coeffs],
    const float in_gain,
    float gains[max_channels]);

void compute_panning_gains_bf(
    const int num_channels,
    const float coeffs[max_ambi_coeffs],
    const float in_gain,
    float gains[max_channels]);

// Sets channel gains for a first-order ambisonics input channel. The matrix is
// a 1x4 'slice' of a transform matrix for the input channel, used to scale and
// orient the sound samples.
void compute_first_order_gains(
    const ALCdevice* device,
    const float* const matrix,
    const float in_gain,
    float* const out_gains);

void compute_first_order_gains_mc(
    const ChannelConfig* channel_coeffs,
    const int num_channels,
    const float mtx[4],
    const float in_gain,
    float gains[max_channels]);

void compute_first_order_gains_bf(
    const int channel_count,
    const float matrix[4],
    const float in_gain,
    float out_gains[max_channels]);

void alu_mix_data(
    ALCdevice* device,
    void* dst_buffer,
    const int sample_count,
    const float* src_samples);


#endif
