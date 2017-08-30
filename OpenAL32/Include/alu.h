#ifndef _ALU_H_
#define _ALU_H_

#include "alMain.h"
#include "alBuffer.h"
#include "alFilter.h"
#include "alAuxEffectSlot.h"


struct ALsource;
struct ALvoice;
struct ALeffectslot;


typedef union aluMatrixf {
    ALfloat m[4][4];
} aluMatrixf;
extern const aluMatrixf IdentityMatrixf;

inline void aluMatrixfSetRow(aluMatrixf *matrix, ALuint row,
                             ALfloat m0, ALfloat m1, ALfloat m2, ALfloat m3)
{
    matrix->m[row][0] = m0;
    matrix->m[row][1] = m1;
    matrix->m[row][2] = m2;
    matrix->m[row][3] = m3;
}

inline void aluMatrixfSet(aluMatrixf *matrix, ALfloat m00, ALfloat m01, ALfloat m02, ALfloat m03,
                                              ALfloat m10, ALfloat m11, ALfloat m12, ALfloat m13,
                                              ALfloat m20, ALfloat m21, ALfloat m22, ALfloat m23,
                                              ALfloat m30, ALfloat m31, ALfloat m32, ALfloat m33)
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


typedef struct DirectParams {
    ALfilterState low_pass;
    ALfilterState high_pass;

    struct DirectParamsGains {
        ALfloat current[MAX_OUTPUT_CHANNELS];
        ALfloat target[MAX_OUTPUT_CHANNELS];
    } gains;
} DirectParams;

typedef struct SendParams {
    ALfilterState low_pass;
    ALfilterState high_pass;

    struct SendParamsGains {
        ALfloat current[MAX_OUTPUT_CHANNELS];
        ALfloat target[MAX_OUTPUT_CHANNELS];
    } gains;
} SendParams;


struct ALvoiceProps {
    struct ALvoiceProps* next;

    ALfloat stereo_pan[2];

    ALfloat radius;

    /** Direct filter and auxiliary send info. */
    struct Direct {
        ALfloat gain;
        ALfloat gain_hf;
        ALfloat hf_reference;
        ALfloat gain_lf;
        ALfloat lf_reference;
    } direct;
    struct Send {
        struct ALeffectslot *slot;
        ALfloat gain;
        ALfloat gain_hf;
        ALfloat hf_reference;
        ALfloat gain_lf;
        ALfloat lf_reference;
    } send[];
};

typedef struct ALvoice {
    struct ALvoiceProps *props;

    struct ALsource* source;
    bool playing;

    /**
     * Number of channels and bytes-per-sample for the attached source's
     * buffer(s).
     */
    ALsizei num_channels;

    struct VoiceDirect {
        enum ActiveFilters filter_type;
        DirectParams params[MAX_INPUT_CHANNELS];

        ALfloat (*buffer)[BUFFERSIZE];
        ALsizei channels;
        ALsizei channels_per_order[MAX_AMBI_ORDER+1];
    } direct;

    struct VoiceSend {
        enum ActiveFilters filter_type;
        SendParams params[MAX_INPUT_CHANNELS];

        ALfloat (*buffer)[BUFFERSIZE];
        ALsizei channels;
    } send[];
} ALvoice;

void DeinitVoice(ALvoice *voice);


typedef void (*MixerFunc)(const ALfloat *data, ALsizei OutChans,
                          ALfloat (*OutBuffer)[BUFFERSIZE], ALfloat *CurrentGains,
                          const ALfloat *TargetGains, ALsizei Counter, ALsizei OutPos,
                          ALsizei BufferSize);
typedef void (*RowMixerFunc)(ALfloat *OutBuffer, const ALfloat *gains,
                             const ALfloat (*data)[BUFFERSIZE], ALsizei InChans,
                             ALsizei InPos, ALsizei BufferSize);


#define GAIN_MIX_MAX  (16.0f) /* +24dB */

#define GAIN_SILENCE_THRESHOLD  (0.00001f) /* -100dB */

#define SPEEDOFSOUNDMETRESPERSEC  (343.3f)

/* Target gain for the reverb decay feedback reaching the decay time. */
#define REVERB_DECAY_GAIN  (0.001f) /* -60 dB */


inline ALfloat minf(ALfloat a, ALfloat b)
{ return ((a > b) ? b : a); }
inline ALfloat maxf(ALfloat a, ALfloat b)
{ return ((a > b) ? a : b); }
inline ALfloat clampf(ALfloat val, ALfloat min, ALfloat max)
{ return minf(max, maxf(min, val)); }

inline ALuint minu(ALuint a, ALuint b)
{ return ((a > b) ? b : a); }
inline ALuint maxu(ALuint a, ALuint b)
{ return ((a > b) ? a : b); }
inline ALuint clampu(ALuint val, ALuint min, ALuint max)
{ return minu(max, maxu(min, val)); }

inline ALint mini(ALint a, ALint b)
{ return ((a > b) ? b : a); }
inline ALint maxi(ALint a, ALint b)
{ return ((a > b) ? a : b); }
inline ALint clampi(ALint val, ALint min, ALint max)
{ return mini(max, maxi(min, val)); }


inline ALfloat lerp(ALfloat val1, ALfloat val2, ALfloat mu)
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
void CalcDirectionCoeffs(const ALfloat dir[3], ALfloat spread, ALfloat coeffs[MAX_AMBI_COEFFS]);

/**
 * CalcAngleCoeffs
 *
 * Calculates ambisonic coefficients based on azimuth and elevation. The
 * azimuth and elevation parameters are in radians, going right and up
 * respectively.
 */
inline void CalcAngleCoeffs(ALfloat azimuth, ALfloat elevation, ALfloat spread, ALfloat coeffs[MAX_AMBI_COEFFS])
{
    ALfloat dir[3] = {
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
#define ComputeAmbientGains(b, g, o) do {                                     \
    if((b).coeff_count > 0)                                                    \
        ComputeAmbientGainsMC((b).ambi.coeffs, (b).num_channels, g, o);        \
    else                                                                      \
        ComputeAmbientGainsBF((b).ambi.map, (b).num_channels, g, o);           \
} while (0)
void ComputeAmbientGainsMC(const ChannelConfig *chancoeffs, ALsizei numchans, ALfloat ingain, ALfloat gains[MAX_OUTPUT_CHANNELS]);
void ComputeAmbientGainsBF(const BFChannelConfig *chanmap, ALsizei numchans, ALfloat ingain, ALfloat gains[MAX_OUTPUT_CHANNELS]);

/**
 * ComputePanningGains
 *
 * Computes panning gains using the given channel decoder coefficients and the
 * pre-calculated direction or angle coefficients.
 */
#define ComputePanningGains(b, c, g, o) do {                                  \
    if((b).coeff_count > 0)                                                    \
        ComputePanningGainsMC((b).ambi.coeffs, (b).num_channels, (b).coeff_count, c, g, o);\
    else                                                                      \
        ComputePanningGainsBF((b).ambi.map, (b).num_channels, c, g, o);        \
} while (0)
void ComputePanningGainsMC(const ChannelConfig *chancoeffs, ALsizei numchans, ALsizei numcoeffs, const ALfloat coeffs[MAX_AMBI_COEFFS], ALfloat ingain, ALfloat gains[MAX_OUTPUT_CHANNELS]);
void ComputePanningGainsBF(const BFChannelConfig *chanmap, ALsizei numchans, const ALfloat coeffs[MAX_AMBI_COEFFS], ALfloat ingain, ALfloat gains[MAX_OUTPUT_CHANNELS]);

/**
 * ComputeFirstOrderGains
 *
 * Sets channel gains for a first-order ambisonics input channel. The matrix is
 * a 1x4 'slice' of a transform matrix for the input channel, used to scale and
 * orient the sound samples.
 */
#define ComputeFirstOrderGains(b, m, g, o) do {                               \
    if((b).coeff_count > 0)                                                    \
        ComputeFirstOrderGainsMC((b).ambi.coeffs, (b).num_channels, m, g, o);  \
    else                                                                      \
        ComputeFirstOrderGainsBF((b).ambi.map, (b).num_channels, m, g, o);     \
} while (0)
void ComputeFirstOrderGainsMC(const ChannelConfig *chancoeffs, ALsizei numchans, const ALfloat mtx[4], ALfloat ingain, ALfloat gains[MAX_OUTPUT_CHANNELS]);
void ComputeFirstOrderGainsBF(const BFChannelConfig *chanmap, ALsizei numchans, const ALfloat mtx[4], ALfloat ingain, ALfloat gains[MAX_OUTPUT_CHANNELS]);


ALboolean MixSource(struct ALvoice *voice, struct ALsource *Source, ALCdevice *Device, ALsizei SamplesToDo);

void aluMixData(ALCdevice *device, ALvoid *OutBuffer, ALsizei NumSamples, const ALfloat* src_samples);
/* Caller must lock the device. */
void aluHandleDisconnect(ALCdevice *device);


#endif
