#ifndef _ALU_H_
#define _ALU_H_

#include "alMain.h"
#include "alBuffer.h"
#include "alFilter.h"
#include "alAuxEffectSlot.h"


struct ALsource;
struct ALvoice;
struct ALeffectslot;


union aluMatrixf
{
    float m[4][4];
}; // aluMatrixf

extern const aluMatrixf IdentityMatrixf;

inline void aluMatrixfSetRow(aluMatrixf *matrix, int row,
                             float m0, float m1, float m2, float m3)
{
    matrix->m[row][0] = m0;
    matrix->m[row][1] = m1;
    matrix->m[row][2] = m2;
    matrix->m[row][3] = m3;
}

inline void aluMatrixfSet(aluMatrixf *matrix, float m00, float m01, float m02, float m03,
                                              float m10, float m11, float m12, float m13,
                                              float m20, float m21, float m22, float m23,
                                              float m30, float m31, float m32, float m33)
{
    aluMatrixfSetRow(matrix, 0, m00, m01, m02, m03);
    aluMatrixfSetRow(matrix, 1, m10, m11, m12, m13);
    aluMatrixfSetRow(matrix, 2, m20, m21, m22, m23);
    aluMatrixfSetRow(matrix, 3, m30, m31, m32, m33);
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
        float current[MAX_OUTPUT_CHANNELS];
        float target[MAX_OUTPUT_CHANNELS];
    }; // Gains


    ALfilterState low_pass;
    ALfilterState high_pass;
    Gains gains;
}; // DirectParams

struct SendParams
{
    struct Gains
    {
        float current[MAX_OUTPUT_CHANNELS];
        float target[MAX_OUTPUT_CHANNELS];
    }; // Gains


    ALfilterState low_pass;
    ALfilterState high_pass;
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
        DirectParams params[MAX_INPUT_CHANNELS];
        SampleBuffers* buffer;
        int channels;
        int channels_per_order[MAX_AMBI_ORDER + 1];
    }; // Direct

    struct Send
    {
        ActiveFilters filter_type;
        SendParams params[MAX_INPUT_CHANNELS];
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

void DeinitVoice(ALvoice *voice);


using MixerFunc = void (*)(const float *data, int OutChans,
                          SampleBuffers& OutBuffer, float *CurrentGains,
                          const float *TargetGains, int Counter, int OutPos,
                          int BufferSize);

using RowMixerFunc = void (*)(float *OutBuffer, const float *gains,
                             const SampleBuffers& data, int InChans,
                             int InPos, int BufferSize);


constexpr auto GAIN_MIX_MAX = 16.0F; /* +24dB */

constexpr auto GAIN_SILENCE_THRESHOLD = 0.00001F; /* -100dB */

constexpr auto SPEEDOFSOUNDMETRESPERSEC = 343.3F;

/* Target gain for the reverb decay feedback reaching the decay time. */
constexpr auto REVERB_DECAY_GAIN = 0.001F; /* -60 dB */


inline float minf(float a, float b)
{ return ((a > b) ? b : a); }
inline float maxf(float a, float b)
{ return ((a > b) ? a : b); }
inline float clampf(float val, float min, float max)
{ return minf(max, maxf(min, val)); }

inline ALint mini(ALint a, ALint b)
{ return ((a > b) ? b : a); }
inline ALint maxi(ALint a, ALint b)
{ return ((a > b) ? a : b); }
inline ALint clampi(ALint val, ALint min, ALint max)
{ return mini(max, maxi(min, val)); }


inline float lerp(float val1, float val2, float mu)
{
    return val1 + (val2-val1)*mu;
}


/* aluInitRenderer
 *
 * Set up the appropriate panning method and mixing method given the device
 * properties.
 */
void aluInitRenderer(ALCdevice *device);

void aluInitEffectPanning(struct ALeffectslot *slot);

/**
 * CalcDirectionCoeffs
 *
 * Calculates ambisonic coefficients based on a direction vector. The vector
 * must be normalized (unit length), and the spread is the angular width of the
 * sound (0...tau).
 */
void CalcDirectionCoeffs(const float dir[3], float spread, float coeffs[MAX_AMBI_COEFFS]);

/**
 * CalcAngleCoeffs
 *
 * Calculates ambisonic coefficients based on azimuth and elevation. The
 * azimuth and elevation parameters are in radians, going right and up
 * respectively.
 */
inline void CalcAngleCoeffs(float azimuth, float elevation, float spread, float coeffs[MAX_AMBI_COEFFS])
{
    float dir[3] = {
        sinf(azimuth) * cosf(elevation),
        sinf(elevation),
        -cosf(azimuth) * cosf(elevation)
    };
    CalcDirectionCoeffs(dir, spread, coeffs);
}

/**
 * ComputeAmbientGains
 *
 * Computes channel gains for ambient, omni-directional sounds.
 */
template<typename T>
void ComputeAmbientGains(
    const T& b,
    const float g,
    float* const o)
{
    if (b.coeff_count > 0)
    {
        ComputeAmbientGainsMC(b.ambi.coeffs.data(), b.num_channels, g, o);
    }
    else
    {
        ComputeAmbientGainsBF(b.ambi.map.data(), b.num_channels, g, o);
    }
}

void ComputeAmbientGainsMC(const ChannelConfig *chancoeffs, int numchans, float ingain, float gains[MAX_OUTPUT_CHANNELS]);
void ComputeAmbientGainsBF(const BFChannelConfig *chanmap, int numchans, float ingain, float gains[MAX_OUTPUT_CHANNELS]);

/**
 * ComputePanningGains
 *
 * Computes panning gains using the given channel decoder coefficients and the
 * pre-calculated direction or angle coefficients.
 */
template<typename T>
void ComputePanningGains(
    const T& b,
    const float* const c,
    const float g,
    float* const o)
{
    if (b.coeff_count > 0)
    {
        ComputePanningGainsMC(b.ambi.coeffs.data(), b.num_channels, b.coeff_count, c, g, o);
    }
    else
    {
        ComputePanningGainsBF(b.ambi.map.data(), b.num_channels, c, g, o);
    }
}

void ComputePanningGainsMC(const ChannelConfig *chancoeffs, int numchans, int numcoeffs, const float coeffs[MAX_AMBI_COEFFS], float ingain, float gains[MAX_OUTPUT_CHANNELS]);
void ComputePanningGainsBF(const BFChannelConfig *chanmap, int numchans, const float coeffs[MAX_AMBI_COEFFS], float ingain, float gains[MAX_OUTPUT_CHANNELS]);

/**
 * ComputeFirstOrderGains
 *
 * Sets channel gains for a first-order ambisonics input channel. The matrix is
 * a 1x4 'slice' of a transform matrix for the input channel, used to scale and
 * orient the sound samples.
 */
template<typename T>
void ComputeFirstOrderGains(
    const T& b,
    const float* const m,
    const float g,
    float* const o)
{
    if (b.coeff_count > 0)
    {
        ComputeFirstOrderGainsMC(b.ambi.coeffs.data(), b.num_channels, m, g, o);
    }
    else
    {
        ComputeFirstOrderGainsBF(b.ambi.map.data(), b.num_channels, m, g, o);
    }
}

void ComputeFirstOrderGainsMC(const ChannelConfig *chancoeffs, int numchans, const float mtx[4], float ingain, float gains[MAX_OUTPUT_CHANNELS]);
void ComputeFirstOrderGainsBF(const BFChannelConfig *chanmap, int numchans, const float mtx[4], float ingain, float gains[MAX_OUTPUT_CHANNELS]);


ALboolean MixSource(struct ALvoice *voice, struct ALsource *Source, ALCdevice *Device, int SamplesToDo);

void aluMixData(ALCdevice *device, void *OutBuffer, int NumSamples, const float* src_samples);


#endif
