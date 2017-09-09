#ifndef _ALU_H_
#define _ALU_H_


#include "alMain.h"
#include "alBuffer.h"
#include "alFilter.h"
#include "alAuxEffectSlot.h"


struct ALsource;
struct ALvoice;
struct ALeffectslot;


struct aluMatrixf
{
    using Items = float[4][4];

    Items m;
}; // aluMatrixf

extern const aluMatrixf identity_matrix_f;

inline void alu_matrix_f_set_row(aluMatrixf *matrix, int row,
                             float m0, float m1, float m2, float m3)
{
    matrix->m[row][0] = m0;
    matrix->m[row][1] = m1;
    matrix->m[row][2] = m2;
    matrix->m[row][3] = m3;
}

inline void alu_matrix_f_set(aluMatrixf *matrix, float m00, float m01, float m02, float m03,
                                              float m10, float m11, float m12, float m13,
                                              float m20, float m21, float m22, float m23,
                                              float m30, float m31, float m32, float m33)
{
    alu_matrix_f_set_row(matrix, 0, m00, m01, m02, m03);
    alu_matrix_f_set_row(matrix, 1, m10, m11, m12, m13);
    alu_matrix_f_set_row(matrix, 2, m20, m21, m22, m23);
    alu_matrix_f_set_row(matrix, 3, m30, m31, m32, m33);
}


enum ActiveFilters {
    AF_None = 0,
    AF_LowPass = 1,
    AF_HighPass = 2,
    AF_BandPass = AF_LowPass | AF_HighPass
};


struct DirectParams
{
    struct Gains
    {
        float current[max_output_channels];
        float target[max_output_channels];
    }; // Gains


    FilterState low_pass;
    FilterState high_pass;
    Gains gains;
}; // DirectParams

struct SendParams
{
    struct Gains
    {
        float current[max_output_channels];
        float target[max_output_channels];
    }; // Gains


    FilterState low_pass;
    FilterState high_pass;
    Gains gains;
}; // SendParams


struct ALvoiceProps
{
    struct Direct
    {
        float gain;
        float gain_hf;
        float hf_reference;
        float gain_lf;
        float lf_reference;
    }; // Direct

    struct Send
    {
        struct ALeffectslot *slot;
        float gain;
        float gain_hf;
        float hf_reference;
        float gain_lf;
        float lf_reference;
    }; // Send

    struct ALvoiceProps* next;
    float stereo_pan[2];
    float radius;

    // Direct filter and auxiliary send info.
    Direct direct;
    Send send;
};

struct ALvoice
{
    struct Direct
    {
        ActiveFilters filter_type;
        DirectParams params[max_input_channels];
        SampleBuffers* buffer;
        int channels;
        int channels_per_order[max_ambi_order + 1];
    }; // Direct

    struct Send
    {
        ActiveFilters filter_type;
        SendParams params[max_input_channels];
        SampleBuffers* buffer;
        int channels;
    }; // Send


    ALvoiceProps props;
    struct ALsource* source;
    bool playing;

    // Number of channels and bytes-per-sample for the attached source's
    // buffer(s).
    int num_channels;

    Direct direct;
    Send send;
}; // ALvoice

void deinit_voice(ALvoice* voice);


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

inline float lerp(
    const float val1,
    const float val2,
    const float mu)
{
    return val1 + ((val2 - val1) * mu);
}


/* aluInitRenderer
 *
 * Set up the appropriate panning method and mixing method given the device
 * properties.
 */
void alu_init_renderer(ALCdevice* device);

void alu_init_effect_panning(struct ALeffectslot* slot);

/**
 * CalcDirectionCoeffs
 *
 * Calculates ambisonic coefficients based on a direction vector. The vector
 * must be normalized (unit length), and the spread is the angular width of the
 * sound (0...tau).
 */
void calc_direction_coeffs(const float dir[3], float spread, float coeffs[max_ambi_coeffs]);

/**
 * CalcAngleCoeffs
 *
 * Calculates ambisonic coefficients based on azimuth and elevation. The
 * azimuth and elevation parameters are in radians, going right and up
 * respectively.
 */
inline void calc_angle_coeffs(float azimuth, float elevation, float spread, float coeffs[max_ambi_coeffs])
{
    float dir[3] = {
        std::sin(azimuth) * std::cos(elevation),
        std::sin(elevation),
        -std::cos(azimuth) * std::cos(elevation)
    };
    calc_direction_coeffs(dir, spread, coeffs);
}

/**
 * ComputeAmbientGains
 *
 * Computes channel gains for ambient, omni-directional sounds.
 */
template<typename T>
void compute_ambient_gains(
    const T& b,
    const float g,
    float* const o)
{
    if (b.coeff_count > 0)
    {
        compute_ambient_gains_mc(b.ambi.coeffs.data(), b.num_channels, g, o);
    }
    else
    {
        compute_ambient_gains_bf(b.ambi.map.data(), b.num_channels, g, o);
    }
}

void compute_ambient_gains_mc(const ChannelConfig *chancoeffs, int numchans, float ingain, float gains[max_output_channels]);
void compute_ambient_gains_bf(const BFChannelConfig *chanmap, int numchans, float ingain, float gains[max_output_channels]);

/**
 * ComputePanningGains
 *
 * Computes panning gains using the given channel decoder coefficients and the
 * pre-calculated direction or angle coefficients.
 */
template<typename T>
void compute_panning_gains(
    const T& b,
    const float* const c,
    const float g,
    float* const o)
{
    if (b.coeff_count > 0)
    {
        compute_panning_gains_mc(b.ambi.coeffs.data(), b.num_channels, b.coeff_count, c, g, o);
    }
    else
    {
        compute_panning_gains_bf(b.ambi.map.data(), b.num_channels, c, g, o);
    }
}

void compute_panning_gains_mc(
    const ChannelConfig* chan_coeffs,
    int num_chans,
    int num_coeffs,
    const float coeffs[max_ambi_coeffs],
    float in_gain,
    float gains[max_output_channels]);

void compute_panning_gains_bf(
    const BFChannelConfig* chan_map,
    int num_chans,
    const float coeffs[max_ambi_coeffs],
    float in_gain,
    float gains[max_output_channels]);

/**
 * ComputeFirstOrderGains
 *
 * Sets channel gains for a first-order ambisonics input channel. The matrix is
 * a 1x4 'slice' of a transform matrix for the input channel, used to scale and
 * orient the sound samples.
 */
template<typename T>
void compute_first_order_gains(
    const T& b,
    const float* const m,
    const float g,
    float* const o)
{
    if (b.coeff_count > 0)
    {
        compute_first_order_gains_mc(b.ambi.coeffs.data(), b.num_channels, m, g, o);
    }
    else
    {
        compute_first_order_gains_bf(b.ambi.map.data(), b.num_channels, m, g, o);
    }
}

void compute_first_order_gains_mc(
    const ChannelConfig* chan_coeffs,
    int num_chans,
    const float mtx[4],
    float in_gain,
    float gains[max_output_channels]);

void compute_first_order_gains_bf(
    const BFChannelConfig* chan_map,
    int num_chans,
    const float mtx[4],
    float in_gain,
    float gains[max_output_channels]);


bool mix_source(struct ALvoice* voice, struct ALsource* source, ALCdevice* device, int samples_to_do);

void alu_mix_data(ALCdevice* device, void* out_buffer, int num_samples, const float* src_samples);


#endif
