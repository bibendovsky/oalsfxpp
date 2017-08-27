#ifndef MIXER_DEFS_H
#define MIXER_DEFS_H

#include "AL/alc.h"
#include "AL/al.h"
#include "alMain.h"
#include "alu.h"

struct MixGains;


/* C resamplers */
const ALfloat *Resample_copy32_C(const InterpState *state, const ALfloat *restrict src, ALsizei frac, ALint increment, ALfloat *restrict dst, ALsizei dstlen);

/* C mixers */
void Mix_C(const ALfloat *data, ALsizei OutChans, ALfloat (*restrict OutBuffer)[BUFFERSIZE],
           ALfloat *CurrentGains, const ALfloat *TargetGains, ALsizei Counter, ALsizei OutPos,
           ALsizei BufferSize);
void MixRow_C(ALfloat *OutBuffer, const ALfloat *Gains,
              const ALfloat (*restrict data)[BUFFERSIZE], ALsizei InChans,
              ALsizei InPos, ALsizei BufferSize);

/* SSE mixers */
void Mix_SSE(const ALfloat *data, ALsizei OutChans, ALfloat (*restrict OutBuffer)[BUFFERSIZE],
             ALfloat *CurrentGains, const ALfloat *TargetGains, ALsizei Counter, ALsizei OutPos,
             ALsizei BufferSize);
void MixRow_SSE(ALfloat *OutBuffer, const ALfloat *Gains,
                const ALfloat (*restrict data)[BUFFERSIZE], ALsizei InChans,
                ALsizei InPos, ALsizei BufferSize);

/* SSE resamplers */
inline void InitiatePositionArrays(ALsizei frac, ALint increment, ALsizei *restrict frac_arr, ALint *restrict pos_arr, ALsizei size)
{
    ALsizei i;

    pos_arr[0] = 0;
    frac_arr[0] = frac;
    for(i = 1;i < size;i++)
    {
        ALint frac_tmp = frac_arr[i-1] + increment;
        pos_arr[i] = pos_arr[i-1] + (frac_tmp>>FRACTIONBITS);
        frac_arr[i] = frac_tmp&FRACTIONMASK;
    }
}

const ALfloat *Resample_lerp32_SSE2(const InterpState *state, const ALfloat *restrict src,
                                    ALsizei frac, ALint increment, ALfloat *restrict dst,
                                    ALsizei numsamples);
const ALfloat *Resample_lerp32_SSE41(const InterpState *state, const ALfloat *restrict src,
                                     ALsizei frac, ALint increment, ALfloat *restrict dst,
                                     ALsizei numsamples);

const ALfloat *Resample_fir4_32_SSE3(const InterpState *state, const ALfloat *restrict src,
                                     ALsizei frac, ALint increment, ALfloat *restrict dst,
                                     ALsizei numsamples);
const ALfloat *Resample_fir4_32_SSE41(const InterpState *state, const ALfloat *restrict src,
                                      ALsizei frac, ALint increment, ALfloat *restrict dst,
                                      ALsizei numsamples);

const ALfloat *Resample_bsinc32_SSE(const InterpState *state, const ALfloat *restrict src,
                                    ALsizei frac, ALint increment, ALfloat *restrict dst,
                                    ALsizei dstlen);

/* Neon mixers */
void Mix_Neon(const ALfloat *data, ALsizei OutChans, ALfloat (*restrict OutBuffer)[BUFFERSIZE],
              ALfloat *CurrentGains, const ALfloat *TargetGains, ALsizei Counter, ALsizei OutPos,
              ALsizei BufferSize);
void MixRow_Neon(ALfloat *OutBuffer, const ALfloat *Gains,
                 const ALfloat (*restrict data)[BUFFERSIZE], ALsizei InChans,
                 ALsizei InPos, ALsizei BufferSize);

/* Neon resamplers */
const ALfloat *Resample_lerp32_Neon(const InterpState *state, const ALfloat *restrict src,
                                    ALsizei frac, ALint increment, ALfloat *restrict dst,
                                    ALsizei numsamples);
const ALfloat *Resample_fir4_32_Neon(const InterpState *state, const ALfloat *restrict src,
                                     ALsizei frac, ALint increment, ALfloat *restrict dst,
                                     ALsizei numsamples);
const ALfloat *Resample_bsinc32_Neon(const InterpState *state, const ALfloat *restrict src,
                                     ALsizei frac, ALint increment, ALfloat *restrict dst,
                                     ALsizei dstlen);

#endif /* MIXER_DEFS_H */
