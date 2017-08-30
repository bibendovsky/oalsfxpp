/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2010 by authors.
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

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "alMain.h"
#include "alAuxEffectSlot.h"
#include "alu.h"
#include "bool.h"


extern inline void CalcAngleCoeffs(ALfloat azimuth, ALfloat elevation, ALfloat spread, ALfloat coeffs[MAX_AMBI_COEFFS]);


static const ALsizei FuMa2ACN[MAX_AMBI_COEFFS] = {
    0,  /* W */
    3,  /* X */
    1,  /* Y */
    2,  /* Z */
    6,  /* R */
    7,  /* S */
    5,  /* T */
    8,  /* U */
    4,  /* V */
    12, /* K */
    13, /* L */
    11, /* M */
    14, /* N */
    10, /* O */
    15, /* P */
    9,  /* Q */
};
static const ALsizei ACN2ACN[MAX_AMBI_COEFFS] = {
    0,  1,  2,  3,  4,  5,  6,  7,
    8,  9, 10, 11, 12, 13, 14, 15
};

/* NOTE: These are scale factors as applied to Ambisonics content. Decoder
 * coefficients should be divided by these values to get proper N3D scalings.
 */
static const ALfloat UnitScale[MAX_AMBI_COEFFS] = {
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
};
static const ALfloat SN3D2N3DScale[MAX_AMBI_COEFFS] = {
    1.000000000f, /* ACN  0 (W), sqrt(1) */
    1.732050808f, /* ACN  1 (Y), sqrt(3) */
    1.732050808f, /* ACN  2 (Z), sqrt(3) */
    1.732050808f, /* ACN  3 (X), sqrt(3) */
    2.236067978f, /* ACN  4 (V), sqrt(5) */
    2.236067978f, /* ACN  5 (T), sqrt(5) */
    2.236067978f, /* ACN  6 (R), sqrt(5) */
    2.236067978f, /* ACN  7 (S), sqrt(5) */
    2.236067978f, /* ACN  8 (U), sqrt(5) */
    2.645751311f, /* ACN  9 (Q), sqrt(7) */
    2.645751311f, /* ACN 10 (O), sqrt(7) */
    2.645751311f, /* ACN 11 (M), sqrt(7) */
    2.645751311f, /* ACN 12 (K), sqrt(7) */
    2.645751311f, /* ACN 13 (L), sqrt(7) */
    2.645751311f, /* ACN 14 (N), sqrt(7) */
    2.645751311f, /* ACN 15 (P), sqrt(7) */
};
static const ALfloat FuMa2N3DScale[MAX_AMBI_COEFFS] = {
    1.414213562f, /* ACN  0 (W), sqrt(2) */
    1.732050808f, /* ACN  1 (Y), sqrt(3) */
    1.732050808f, /* ACN  2 (Z), sqrt(3) */
    1.732050808f, /* ACN  3 (X), sqrt(3) */
    1.936491673f, /* ACN  4 (V), sqrt(15)/2 */
    1.936491673f, /* ACN  5 (T), sqrt(15)/2 */
    2.236067978f, /* ACN  6 (R), sqrt(5) */
    1.936491673f, /* ACN  7 (S), sqrt(15)/2 */
    1.936491673f, /* ACN  8 (U), sqrt(15)/2 */
    2.091650066f, /* ACN  9 (Q), sqrt(35/8) */
    1.972026594f, /* ACN 10 (O), sqrt(35)/3 */
    2.231093404f, /* ACN 11 (M), sqrt(224/45) */
    2.645751311f, /* ACN 12 (K), sqrt(7) */
    2.231093404f, /* ACN 13 (L), sqrt(224/45) */
    1.972026594f, /* ACN 14 (N), sqrt(35)/3 */
    2.091650066f, /* ACN 15 (P), sqrt(35/8) */
};


void CalcDirectionCoeffs(const ALfloat dir[3], ALfloat spread, ALfloat coeffs[MAX_AMBI_COEFFS])
{
    /* Convert from OpenAL coords to Ambisonics. */
    ALfloat x = -dir[2];
    ALfloat y = -dir[0];
    ALfloat z =  dir[1];

    /* Zeroth-order */
    coeffs[0]  = 1.0f; /* ACN 0 = 1 */
    /* First-order */
    coeffs[1]  = 1.732050808f * y; /* ACN 1 = sqrt(3) * Y */
    coeffs[2]  = 1.732050808f * z; /* ACN 2 = sqrt(3) * Z */
    coeffs[3]  = 1.732050808f * x; /* ACN 3 = sqrt(3) * X */
    /* Second-order */
    coeffs[4]  = 3.872983346f * x * y;             /* ACN 4 = sqrt(15) * X * Y */
    coeffs[5]  = 3.872983346f * y * z;             /* ACN 5 = sqrt(15) * Y * Z */
    coeffs[6]  = 1.118033989f * (3.0f*z*z - 1.0f); /* ACN 6 = sqrt(5)/2 * (3*Z*Z - 1) */
    coeffs[7]  = 3.872983346f * x * z;             /* ACN 7 = sqrt(15) * X * Z */
    coeffs[8]  = 1.936491673f * (x*x - y*y);       /* ACN 8 = sqrt(15)/2 * (X*X - Y*Y) */
    /* Third-order */
    coeffs[9]  =  2.091650066f * y * (3.0f*x*x - y*y);  /* ACN  9 = sqrt(35/8) * Y * (3*X*X - Y*Y) */
    coeffs[10] = 10.246950766f * z * x * y;             /* ACN 10 = sqrt(105) * Z * X * Y */
    coeffs[11] =  1.620185175f * y * (5.0f*z*z - 1.0f); /* ACN 11 = sqrt(21/8) * Y * (5*Z*Z - 1) */
    coeffs[12] =  1.322875656f * z * (5.0f*z*z - 3.0f); /* ACN 12 = sqrt(7)/2 * Z * (5*Z*Z - 3) */
    coeffs[13] =  1.620185175f * x * (5.0f*z*z - 1.0f); /* ACN 13 = sqrt(21/8) * X * (5*Z*Z - 1) */
    coeffs[14] =  5.123475383f * z * (x*x - y*y);       /* ACN 14 = sqrt(105)/2 * Z * (X*X - Y*Y) */
    coeffs[15] =  2.091650066f * x * (x*x - 3.0f*y*y);  /* ACN 15 = sqrt(35/8) * X * (X*X - 3*Y*Y) */

    if(spread > 0.0f)
    {
        /* Implement the spread by using a spherical source that subtends the
         * angle spread. See:
         * http://www.ppsloan.org/publications/StupidSH36.pdf - Appendix A3
         *
         * When adjusted for N3D normalization instead of SN3D, these
         * calculations are:
         *
         * ZH0 = -sqrt(pi) * (-1+ca);
         * ZH1 =  0.5*sqrt(pi) * sa*sa;
         * ZH2 = -0.5*sqrt(pi) * ca*(-1+ca)*(ca+1);
         * ZH3 = -0.125*sqrt(pi) * (-1+ca)*(ca+1)*(5*ca*ca - 1);
         * ZH4 = -0.125*sqrt(pi) * ca*(-1+ca)*(ca+1)*(7*ca*ca - 3);
         * ZH5 = -0.0625*sqrt(pi) * (-1+ca)*(ca+1)*(21*ca*ca*ca*ca - 14*ca*ca + 1);
         *
         * The gain of the source is compensated for size, so that the
         * loundness doesn't depend on the spread. Thus:
         *
         * ZH0 = 1.0f;
         * ZH1 = 0.5f * (ca+1.0f);
         * ZH2 = 0.5f * (ca+1.0f)*ca;
         * ZH3 = 0.125f * (ca+1.0f)*(5.0f*ca*ca - 1.0f);
         * ZH4 = 0.125f * (ca+1.0f)*(7.0f*ca*ca - 3.0f)*ca;
         * ZH5 = 0.0625f * (ca+1.0f)*(21.0f*ca*ca*ca*ca - 14.0f*ca*ca + 1.0f);
         */
        ALfloat ca = cosf(spread * 0.5f);
        /* Increase the source volume by up to +3dB for a full spread. */
        ALfloat scale = sqrtf(1.0f + spread/F_TAU);

        ALfloat ZH0_norm = scale;
        ALfloat ZH1_norm = 0.5f * (ca+1.f) * scale;
        ALfloat ZH2_norm = 0.5f * (ca+1.f)*ca * scale;
        ALfloat ZH3_norm = 0.125f * (ca+1.f)*(5.f*ca*ca-1.f) * scale;

        /* Zeroth-order */
        coeffs[0]  *= ZH0_norm;
        /* First-order */
        coeffs[1]  *= ZH1_norm;
        coeffs[2]  *= ZH1_norm;
        coeffs[3]  *= ZH1_norm;
        /* Second-order */
        coeffs[4]  *= ZH2_norm;
        coeffs[5]  *= ZH2_norm;
        coeffs[6]  *= ZH2_norm;
        coeffs[7]  *= ZH2_norm;
        coeffs[8]  *= ZH2_norm;
        /* Third-order */
        coeffs[9]  *= ZH3_norm;
        coeffs[10] *= ZH3_norm;
        coeffs[11] *= ZH3_norm;
        coeffs[12] *= ZH3_norm;
        coeffs[13] *= ZH3_norm;
        coeffs[14] *= ZH3_norm;
        coeffs[15] *= ZH3_norm;
    }
}

void ComputeAmbientGainsMC(const ChannelConfig *chancoeffs, ALsizei numchans, ALfloat ingain, ALfloat gains[MAX_OUTPUT_CHANNELS])
{
    ALsizei i;

    for(i = 0;i < numchans;i++)
        gains[i] = chancoeffs[i][0] * 1.414213562f * ingain;
    for(;i < MAX_OUTPUT_CHANNELS;i++)
        gains[i] = 0.0f;
}

void ComputeAmbientGainsBF(const BFChannelConfig *chanmap, ALsizei numchans, ALfloat ingain, ALfloat gains[MAX_OUTPUT_CHANNELS])
{
    ALfloat gain = 0.0f;
    ALsizei i;

    for(i = 0;i < numchans;i++)
    {
        if(chanmap[i].Index == 0)
            gain += chanmap[i].Scale;
    }
    gains[0] = gain * 1.414213562f * ingain;
    for(i = 1;i < MAX_OUTPUT_CHANNELS;i++)
        gains[i] = 0.0f;
}

void ComputePanningGainsMC(const ChannelConfig *chancoeffs, ALsizei numchans, ALsizei numcoeffs, const ALfloat coeffs[MAX_AMBI_COEFFS], ALfloat ingain, ALfloat gains[MAX_OUTPUT_CHANNELS])
{
    ALsizei i, j;

    for(i = 0;i < numchans;i++)
    {
        float gain = 0.0f;
        for(j = 0;j < numcoeffs;j++)
            gain += chancoeffs[i][j]*coeffs[j];
        gains[i] = clampf(gain, 0.0f, 1.0f) * ingain;
    }
    for(;i < MAX_OUTPUT_CHANNELS;i++)
        gains[i] = 0.0f;
}

void ComputePanningGainsBF(const BFChannelConfig *chanmap, ALsizei numchans, const ALfloat coeffs[MAX_AMBI_COEFFS], ALfloat ingain, ALfloat gains[MAX_OUTPUT_CHANNELS])
{
    ALsizei i;

    for(i = 0;i < numchans;i++)
        gains[i] = chanmap[i].Scale * coeffs[chanmap[i].Index] * ingain;
    for(;i < MAX_OUTPUT_CHANNELS;i++)
        gains[i] = 0.0f;
}

void ComputeFirstOrderGainsMC(const ChannelConfig *chancoeffs, ALsizei numchans, const ALfloat mtx[4], ALfloat ingain, ALfloat gains[MAX_OUTPUT_CHANNELS])
{
    ALsizei i, j;

    for(i = 0;i < numchans;i++)
    {
        float gain = 0.0f;
        for(j = 0;j < 4;j++)
            gain += chancoeffs[i][j] * mtx[j];
        gains[i] = clampf(gain, 0.0f, 1.0f) * ingain;
    }
    for(;i < MAX_OUTPUT_CHANNELS;i++)
        gains[i] = 0.0f;
}

void ComputeFirstOrderGainsBF(const BFChannelConfig *chanmap, ALsizei numchans, const ALfloat mtx[4], ALfloat ingain, ALfloat gains[MAX_OUTPUT_CHANNELS])
{
    ALsizei i;

    for(i = 0;i < numchans;i++)
        gains[i] = chanmap[i].Scale * mtx[chanmap[i].Index] * ingain;
    for(;i < MAX_OUTPUT_CHANNELS;i++)
        gains[i] = 0.0f;
}


static inline const char *GetLabelFromChannel(enum Channel channel)
{
    switch(channel)
    {
        case FrontLeft: return "front-left";
        case FrontRight: return "front-right";
        case FrontCenter: return "front-center";
        case LFE: return "lfe";
        case BackLeft: return "back-left";
        case BackRight: return "back-right";
        case BackCenter: return "back-center";
        case SideLeft: return "side-left";
        case SideRight: return "side-right";

        case UpperFrontLeft: return "upper-front-left";
        case UpperFrontRight: return "upper-front-right";
        case UpperBackLeft: return "upper-back-left";
        case UpperBackRight: return "upper-back-right";
        case LowerFrontLeft: return "lower-front-left";
        case LowerFrontRight: return "lower-front-right";
        case LowerBackLeft: return "lower-back-left";
        case LowerBackRight: return "lower-back-right";

        case Aux0: return "aux-0";
        case Aux1: return "aux-1";
        case Aux2: return "aux-2";
        case Aux3: return "aux-3";
        case Aux4: return "aux-4";
        case Aux5: return "aux-5";
        case Aux6: return "aux-6";
        case Aux7: return "aux-7";
        case Aux8: return "aux-8";
        case Aux9: return "aux-9";
        case Aux10: return "aux-10";
        case Aux11: return "aux-11";
        case Aux12: return "aux-12";
        case Aux13: return "aux-13";
        case Aux14: return "aux-14";
        case Aux15: return "aux-15";

        case InvalidChannel: break;
    }
    return "(unknown)";
}


typedef struct ChannelMap {
    enum Channel ChanName;
    ChannelConfig Config;
} ChannelMap;

static void SetChannelMap(const enum Channel *devchans, ChannelConfig *ambicoeffs,
                          const ChannelMap *chanmap, size_t count, ALsizei *outcount)
{
    size_t j, k;
    ALsizei i;

    for(i = 0;i < MAX_OUTPUT_CHANNELS && devchans[i] != InvalidChannel;i++)
    {
        if(devchans[i] == LFE)
        {
            for(j = 0;j < MAX_AMBI_COEFFS;j++)
                ambicoeffs[i][j] = 0.0f;
            continue;
        }

        for(j = 0;j < count;j++)
        {
            if(devchans[i] != chanmap[j].ChanName)
                continue;

            for(k = 0;k < MAX_AMBI_COEFFS;++k)
                ambicoeffs[i][k] = chanmap[j].Config[k];
            break;
        }
    }
    *outcount = i;
}

static const ChannelMap MonoCfg[1] = {
    { FrontCenter, { 1.0f } },
}, StereoCfg[2] = {
    { FrontLeft,   { 5.00000000e-1f,  2.88675135e-1f, 0.0f,  1.19573156e-1f } },
    { FrontRight,  { 5.00000000e-1f, -2.88675135e-1f, 0.0f,  1.19573156e-1f } },
}, QuadCfg[4] = {
    { BackLeft,    { 3.53553391e-1f,  2.04124145e-1f, 0.0f, -2.04124145e-1f } },
    { FrontLeft,   { 3.53553391e-1f,  2.04124145e-1f, 0.0f,  2.04124145e-1f } },
    { FrontRight,  { 3.53553391e-1f, -2.04124145e-1f, 0.0f,  2.04124145e-1f } },
    { BackRight,   { 3.53553391e-1f, -2.04124145e-1f, 0.0f, -2.04124145e-1f } },
}, X51SideCfg[5] = {
    { SideLeft,    { 3.33001372e-1f,  1.89085671e-1f, 0.0f, -2.00041334e-1f, -2.12309737e-2f, 0.0f, 0.0f, 0.0f, -1.14573483e-2f } },
    { FrontLeft,   { 1.47751298e-1f,  1.28994110e-1f, 0.0f,  1.15190495e-1f,  7.44949143e-2f, 0.0f, 0.0f, 0.0f, -6.47739980e-3f } },
    { FrontCenter, { 7.73595729e-2f,  0.00000000e+0f, 0.0f,  9.71390298e-2f,  0.00000000e+0f, 0.0f, 0.0f, 0.0f,  5.18625335e-2f } },
    { FrontRight,  { 1.47751298e-1f, -1.28994110e-1f, 0.0f,  1.15190495e-1f, -7.44949143e-2f, 0.0f, 0.0f, 0.0f, -6.47739980e-3f } },
    { SideRight,   { 3.33001372e-1f, -1.89085671e-1f, 0.0f, -2.00041334e-1f,  2.12309737e-2f, 0.0f, 0.0f, 0.0f, -1.14573483e-2f } },
}, X51RearCfg[5] = {
    { BackLeft,    { 3.33001372e-1f,  1.89085671e-1f, 0.0f, -2.00041334e-1f, -2.12309737e-2f, 0.0f, 0.0f, 0.0f, -1.14573483e-2f } },
    { FrontLeft,   { 1.47751298e-1f,  1.28994110e-1f, 0.0f,  1.15190495e-1f,  7.44949143e-2f, 0.0f, 0.0f, 0.0f, -6.47739980e-3f } },
    { FrontCenter, { 7.73595729e-2f,  0.00000000e+0f, 0.0f,  9.71390298e-2f,  0.00000000e+0f, 0.0f, 0.0f, 0.0f,  5.18625335e-2f } },
    { FrontRight,  { 1.47751298e-1f, -1.28994110e-1f, 0.0f,  1.15190495e-1f, -7.44949143e-2f, 0.0f, 0.0f, 0.0f, -6.47739980e-3f } },
    { BackRight,   { 3.33001372e-1f, -1.89085671e-1f, 0.0f, -2.00041334e-1f,  2.12309737e-2f, 0.0f, 0.0f, 0.0f, -1.14573483e-2f } },
}, X61Cfg[6] = {
    { SideLeft,    { 2.04462744e-1f,  2.17178497e-1f, 0.0f, -4.39990188e-2f, -2.60787329e-2f, 0.0f, 0.0f, 0.0f, -6.87238843e-2f } },
    { FrontLeft,   { 1.18130342e-1f,  9.34633906e-2f, 0.0f,  1.08553749e-1f,  6.80658795e-2f, 0.0f, 0.0f, 0.0f,  1.08999485e-2f } },
    { FrontCenter, { 7.73595729e-2f,  0.00000000e+0f, 0.0f,  9.71390298e-2f,  0.00000000e+0f, 0.0f, 0.0f, 0.0f,  5.18625335e-2f } },
    { FrontRight,  { 1.18130342e-1f, -9.34633906e-2f, 0.0f,  1.08553749e-1f, -6.80658795e-2f, 0.0f, 0.0f, 0.0f,  1.08999485e-2f } },
    { SideRight,   { 2.04462744e-1f, -2.17178497e-1f, 0.0f, -4.39990188e-2f,  2.60787329e-2f, 0.0f, 0.0f, 0.0f, -6.87238843e-2f } },
    { BackCenter,  { 2.50001688e-1f,  0.00000000e+0f, 0.0f, -2.50000094e-1f,  0.00000000e+0f, 0.0f, 0.0f, 0.0f,  6.05133395e-2f } },
}, X71Cfg[6] = {
    { BackLeft,    { 2.04124145e-1f,  1.08880247e-1f, 0.0f, -1.88586120e-1f, -1.29099444e-1f, 0.0f, 0.0f, 0.0f,  7.45355993e-2f,  3.73460789e-2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  0.00000000e+0f } },
    { SideLeft,    { 2.04124145e-1f,  2.17760495e-1f, 0.0f,  0.00000000e+0f,  0.00000000e+0f, 0.0f, 0.0f, 0.0f, -1.49071198e-1f, -3.73460789e-2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  0.00000000e+0f } },
    { FrontLeft,   { 2.04124145e-1f,  1.08880247e-1f, 0.0f,  1.88586120e-1f,  1.29099444e-1f, 0.0f, 0.0f, 0.0f,  7.45355993e-2f,  3.73460789e-2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  0.00000000e+0f } },
    { FrontRight,  { 2.04124145e-1f, -1.08880247e-1f, 0.0f,  1.88586120e-1f, -1.29099444e-1f, 0.0f, 0.0f, 0.0f,  7.45355993e-2f, -3.73460789e-2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  0.00000000e+0f } },
    { SideRight,   { 2.04124145e-1f, -2.17760495e-1f, 0.0f,  0.00000000e+0f,  0.00000000e+0f, 0.0f, 0.0f, 0.0f, -1.49071198e-1f,  3.73460789e-2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  0.00000000e+0f } },
    { BackRight,   { 2.04124145e-1f, -1.08880247e-1f, 0.0f, -1.88586120e-1f,  1.29099444e-1f, 0.0f, 0.0f, 0.0f,  7.45355993e-2f, -3.73460789e-2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,  0.00000000e+0f } },
};

static void InitNearFieldCtrl(ALCdevice *device, ALfloat ctrl_dist, ALsizei order, bool periphonic)
{
}

static void InitPanning(ALCdevice *device)
{
    const ChannelMap *chanmap = NULL;
    ALsizei coeffcount = 0;
    ALsizei count = 0;
    ALsizei i, j;

    switch(device->FmtChans)
    {
        case DevFmtMono:
            count = COUNTOF(MonoCfg);
            chanmap = MonoCfg;
            coeffcount = 1;
            break;

        case DevFmtStereo:
            count = COUNTOF(StereoCfg);
            chanmap = StereoCfg;
            coeffcount = 4;
            break;

        case DevFmtQuad:
            count = COUNTOF(QuadCfg);
            chanmap = QuadCfg;
            coeffcount = 4;
            break;

        case DevFmtX51:
            count = COUNTOF(X51SideCfg);
            chanmap = X51SideCfg;
            coeffcount = 9;
            break;

        case DevFmtX51Rear:
            count = COUNTOF(X51RearCfg);
            chanmap = X51RearCfg;
            coeffcount = 9;
            break;

        case DevFmtX61:
            count = COUNTOF(X61Cfg);
            chanmap = X61Cfg;
            coeffcount = 9;
            break;

        case DevFmtX71:
            count = COUNTOF(X71Cfg);
            chanmap = X71Cfg;
            coeffcount = 16;
            break;
    }

    {
        ALfloat w_scale, xyz_scale;

        SetChannelMap(device->RealOut.ChannelName, device->Dry.Ambi.Coeffs,
                      chanmap, count, &device->Dry.NumChannels);
        device->Dry.CoeffCount = coeffcount;

        w_scale = 1.0f;
        xyz_scale = 1.0f;

        memset(&device->FOAOut.Ambi, 0, sizeof(device->FOAOut.Ambi));
        for(i = 0;i < device->Dry.NumChannels;i++)
        {
            device->FOAOut.Ambi.Coeffs[i][0] = device->Dry.Ambi.Coeffs[i][0] * w_scale;
            for(j = 1;j < 4;j++)
                device->FOAOut.Ambi.Coeffs[i][j] = device->Dry.Ambi.Coeffs[i][j] * xyz_scale;
        }
        device->FOAOut.CoeffCount = 4;
        device->FOAOut.NumChannels = 0;
    }
    device->RealOut.NumChannels = 0;
}

void aluInitRenderer(ALCdevice *device)
{
    size_t i;

    memset(&device->Dry.Ambi, 0, sizeof(device->Dry.Ambi));
    device->Dry.CoeffCount = 0;
    device->Dry.NumChannels = 0;
    for(i = 0;i < MAX_AMBI_ORDER+1;i++)
        device->Dry.NumChannelsPerOrder[i] = 0;

    SetDefaultWFXChannelOrder(device);

    if(device->FmtChans != DevFmtStereo)
    {
        InitPanning(device);
        return;
    }

    InitPanning(device);
}


void aluInitEffectPanning(ALeffectslot *slot)
{
    ALsizei i;

    memset(slot->chan_map, 0, sizeof(slot->chan_map));
    slot->num_channels = 0;

    for(i = 0;i < MAX_EFFECT_CHANNELS;i++)
    {
        slot->chan_map[i].Scale = 1.0f;
        slot->chan_map[i].Index = i;
    }
    slot->num_channels = i;
}
