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
#include "alu.h"


extern inline void CalcAngleCoeffs(float azimuth, float elevation, float spread, float coeffs[max_ambi_coeffs]);


void CalcDirectionCoeffs(const float dir[3], float spread, float coeffs[max_ambi_coeffs])
{
    /* Convert from OpenAL coords to Ambisonics. */
    float x = -dir[2];
    float y = -dir[0];
    float z =  dir[1];

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
        float ca = std::cos(spread * 0.5f);
        /* Increase the source volume by up to +3dB for a full spread. */
        float scale = sqrtf(1.0f + spread/tau);

        float ZH0_norm = scale;
        float ZH1_norm = 0.5f * (ca+1.f) * scale;
        float ZH2_norm = 0.5f * (ca+1.f)*ca * scale;
        float ZH3_norm = 0.125f * (ca+1.f)*(5.f*ca*ca-1.f) * scale;

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

void ComputeAmbientGainsMC(const ChannelConfig *chancoeffs, int numchans, float ingain, float gains[max_output_channels])
{
    int i;

    for(i = 0;i < numchans;i++)
        gains[i] = chancoeffs[i][0] * 1.414213562f * ingain;
    for(;i < max_output_channels;i++)
        gains[i] = 0.0f;
}

void ComputeAmbientGainsBF(const BFChannelConfig *chanmap, int numchans, float ingain, float gains[max_output_channels])
{
    float gain = 0.0f;
    int i;

    for(i = 0;i < numchans;i++)
    {
        if(chanmap[i].index == 0)
            gain += chanmap[i].scale;
    }
    gains[0] = gain * 1.414213562f * ingain;
    for(i = 1;i < max_output_channels;i++)
        gains[i] = 0.0f;
}

void ComputePanningGainsMC(const ChannelConfig *chancoeffs, int numchans, int numcoeffs, const float coeffs[max_ambi_coeffs], float ingain, float gains[max_output_channels])
{
    int i, j;

    for(i = 0;i < numchans;i++)
    {
        float gain = 0.0f;
        for(j = 0;j < numcoeffs;j++)
            gain += chancoeffs[i][j]*coeffs[j];
        gains[i] = clamp(gain, 0.0F, 1.0F) * ingain;
    }
    for(;i < max_output_channels;i++)
        gains[i] = 0.0f;
}

void ComputePanningGainsBF(const BFChannelConfig *chanmap, int numchans, const float coeffs[max_ambi_coeffs], float ingain, float gains[max_output_channels])
{
    int i;

    for(i = 0;i < numchans;i++)
        gains[i] = chanmap[i].scale * coeffs[chanmap[i].index] * ingain;
    for(;i < max_output_channels;i++)
        gains[i] = 0.0f;
}

void ComputeFirstOrderGainsMC(const ChannelConfig *chancoeffs, int numchans, const float mtx[4], float ingain, float gains[max_output_channels])
{
    int i, j;

    for(i = 0;i < numchans;i++)
    {
        float gain = 0.0f;
        for(j = 0;j < 4;j++)
            gain += chancoeffs[i][j] * mtx[j];
        gains[i] = clamp(gain, 0.0F, 1.0F) * ingain;
    }
    for(;i < max_output_channels;i++)
        gains[i] = 0.0f;
}

void ComputeFirstOrderGainsBF(const BFChannelConfig *chanmap, int numchans, const float mtx[4], float ingain, float gains[max_output_channels])
{
    int i;

    for(i = 0;i < numchans;i++)
        gains[i] = chanmap[i].scale * mtx[chanmap[i].index] * ingain;
    for(;i < max_output_channels;i++)
        gains[i] = 0.0f;
}


struct ChannelMap
{
    enum Channel ChanName;
    ChannelConfig Config;
}; // ChannelMap

static void SetChannelMap(const enum Channel *devchans, ChannelConfig *ambicoeffs,
                          const ChannelMap *chanmap, size_t count, int *outcount)
{
    size_t j, k;
    int i;

    for(i = 0;i < max_output_channels && devchans[i] != InvalidChannel;i++)
    {
        if(devchans[i] == LFE)
        {
            for(j = 0;j < max_ambi_coeffs;j++)
                ambicoeffs[i][j] = 0.0f;
            continue;
        }

        for(j = 0;j < count;j++)
        {
            if(devchans[i] != chanmap[j].ChanName)
                continue;

            for(k = 0;k < max_ambi_coeffs;++k)
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

static void InitPanning(ALCdevice *device)
{
    const ChannelMap *chanmap = NULL;
    int coeffcount = 0;
    int count = 0;
    int i, j;

    switch(device->fmt_chans)
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
        float w_scale, xyz_scale;

        SetChannelMap(device->real_out.channel_name.data(), device->dry.ambi.coeffs.data(),
                      chanmap, count, &device->dry.num_channels);
        device->dry.coeff_count = coeffcount;

        w_scale = 1.0f;
        xyz_scale = 1.0f;

        memset(&device->foa_out.ambi, 0, sizeof(device->foa_out.ambi));
        for(i = 0;i < device->dry.num_channels;i++)
        {
            device->foa_out.ambi.coeffs[i][0] = device->dry.ambi.coeffs[i][0] * w_scale;
            for(j = 1;j < 4;j++)
                device->foa_out.ambi.coeffs[i][j] = device->dry.ambi.coeffs[i][j] * xyz_scale;
        }
        device->foa_out.coeff_count = 4;
        device->foa_out.num_channels = 0;
    }
    device->real_out.num_channels = 0;
}

void aluInitRenderer(ALCdevice *device)
{
    size_t i;

    memset(&device->dry.ambi, 0, sizeof(device->dry.ambi));
    device->dry.coeff_count = 0;
    device->dry.num_channels = 0;
    for(i = 0;i < max_ambi_order+1;i++)
        device->dry.num_channels_per_order[i] = 0;

    SetDefaultWFXChannelOrder(device);

    if(device->fmt_chans != DevFmtStereo)
    {
        InitPanning(device);
        return;
    }

    InitPanning(device);
}


void aluInitEffectPanning(ALeffectslot *slot)
{
    int i;

    memset(slot->chan_map, 0, sizeof(slot->chan_map));
    slot->num_channels = 0;

    for(i = 0;i < max_effect_channels;i++)
    {
        slot->chan_map[i].scale = 1.0f;
        slot->chan_map[i].index = i;
    }
    slot->num_channels = i;
}
