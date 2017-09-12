#ifndef _ALU_H_
#define _ALU_H_


#include "alMain.h"
#include "alBuffer.h"
#include "alFilter.h"
#include "alAuxEffectSlot.h"


struct ALsource;
struct ALvoice;
struct EffectSlot;


struct aluMatrixf
{
    using Items = float[4][4];

    Items m_;
}; // aluMatrixf

extern const aluMatrixf identity_matrix_f;

void alu_matrix_f_set_row(
    aluMatrixf* matrix,
    const int row,
    const float m0,
    const float m1,
    const float m2,
    const float m3);

void alu_matrix_f_set(
    aluMatrixf* matrix,
    const float m00,
    const float m01,
    const float m02,
    const float m03,
    const float m10,
    const float m11,
    const float m12,
    const float m13,
    const float m20,
    const float m21,
    const float m22,
    const float m23,
    const float m30,
    const float m31,
    const float m32,
    const float m33);

enum class ActiveFilters
{
    none = 0,
    low_pass = 1,
    high_pass = 2,
    band_pass = low_pass | high_pass
}; // ActiveFilters


struct ParamsBase
{
    struct Gains
    {
        float current_[max_output_channels];
        float target_[max_output_channels];

        void reset();
    }; // Gains


    FilterState low_pass_;
    FilterState high_pass_;
    Gains gains_;


    void reset();
}; // ParamsBase


using DirectParams = ParamsBase;
using SendParams = ParamsBase;


struct ALvoiceProps
{
    struct Base
    {
        float gain_;
        float gain_hf_;
        float hf_reference_;
        float gain_lf_;
        float lf_reference_;
    }; // Base

    struct Send :
        Base
    {
        EffectSlot* effect_slot_;
    }; // Send

    // Direct filter and auxiliary send info.
    Base direct_;
    Send send_;
};

struct ALvoice
{
    struct State
    {
        ActiveFilters filter_type_;
        DirectParams params_[max_input_channels];
        SampleBuffers* buffers_;
        int channel_count_;
    }; // State


    ALvoiceProps props_;
    ALsource* source_;
    bool is_playing_;

    // Number of channels and bytes-per-sample for the attached source's
    // buffer(s).
    int channel_count_;

    State direct_;
    State send_;
}; // ALvoice


void deinit_voice(
    ALvoice* voice);


using MixerFunc = void (*)(
    const float* data,
    int out_chans,
    SampleBuffers& out_buffer,
    float* current_gains,
    const float* target_gains,
    int counter,
    int out_pos,
    int buffer_size);

using RowMixerFunc = void (*)(
    float* out_buffer,
    const float* gains,
    const SampleBuffers& data,
    int in_chans,
    int in_pos,
    int buffer_size);


constexpr auto max_mix_gain = 16.0F; // +24dB

constexpr auto silence_threshold_gain = 0.00001F; // -100dB

constexpr auto speed_of_sound_mps = 343.3F;

// Target gain for the reverb decay feedback reaching the decay time.
constexpr auto reverb_decay_gain = 0.001F; // -60 dB


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

void alu_init_effect_panning(
    EffectSlot* slot);

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
    const ALCdevice* b,
    const float g,
    float* const o);

void compute_ambient_gains_mc(
    const ChannelConfig* channel_coeffs,
    const int num_channels,
    const float in_gain,
    float gains[max_output_channels]);

void compute_ambient_gains_bf(
    const BFChannelConfig* channel_map,
    const int num_channels,
    const float in_gain,
    float gains[max_output_channels]);

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
    float gains[max_output_channels]);

void compute_panning_gains_bf(
    const BFChannelConfig* channel_map,
    const int num_channels,
    const float coeffs[max_ambi_coeffs],
    const float in_gain,
    float gains[max_output_channels]);

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
    float gains[max_output_channels]);

void compute_first_order_gains_bf(
    const BFChannelConfig* channel_map,
    const int num_channels,
    const float mtx[4],
    const float in_gain,
    float gains[max_output_channels]);

bool mix_source(
    ALvoice* voice,
    ALsource* source,
    ALCdevice* device,
    int samples_to_do);

void alu_mix_data(
    ALCdevice* device,
    void* out_buffer,
    const int num_samples,
    const float* src_samples);


#endif
