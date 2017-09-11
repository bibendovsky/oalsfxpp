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


void calc_direction_coeffs(
    const float dir[3],
    const float spread,
    float coeffs[max_ambi_coeffs])
{
    // Convert from OpenAL coords to Ambisonics.
    const auto x = -dir[2];
    const auto y = -dir[0];
    const auto z = dir[1];

    // Zeroth-order
    coeffs[0] = 1.0F; // ACN 0 = 1

    // First-order
    coeffs[1] = 1.732050808F * y; // ACN 1 = sqrt(3) * Y
    coeffs[2] = 1.732050808F * z; // ACN 2 = sqrt(3) * Z
    coeffs[3] = 1.732050808F * x; // ACN 3 = sqrt(3) * X

    // Second-order
    coeffs[4] = 3.872983346F * x * y; // ACN 4 = sqrt(15) * X * Y
    coeffs[5] = 3.872983346F * y * z; // ACN 5 = sqrt(15) * Y * Z
    coeffs[6] = 1.118033989F * ((3.0F * z * z) - 1.0F); // ACN 6 = sqrt(5)/2 * (3*Z*Z - 1)
    coeffs[7] = 3.872983346F * x * z; // ACN 7 = sqrt(15) * X * Z
    coeffs[8] = 1.936491673F * ((x * x) - (y * y)); // ACN 8 = sqrt(15)/2 * (X*X - Y*Y)

    // Third-order
    coeffs[9] = 2.091650066F * y * ((3.0F * x * x) - (y * y)); // ACN  9 = sqrt(35/8) * Y * (3*X*X - Y*Y)
    coeffs[10] = 10.246950766F * z * x * y; // ACN 10 = sqrt(105) * Z * X * Y
    coeffs[11] = 1.620185175F * y * ((5.0F * z * z) - 1.0F); // ACN 11 = sqrt(21/8) * Y * (5*Z*Z - 1)
    coeffs[12] = 1.322875656F * z * ((5.0F * z * z) - 3.0F); // ACN 12 = sqrt(7)/2 * Z * (5*Z*Z - 3)
    coeffs[13] = 1.620185175F * x * ((5.0F * z * z) - 1.0F); // ACN 13 = sqrt(21/8) * X * (5*Z*Z - 1)
    coeffs[14] = 5.123475383F * z * ((x * x) - (y * y)); // ACN 14 = sqrt(105)/2 * Z * (X*X - Y*Y)
    coeffs[15] = 2.091650066F * x * ((x * x) - (3.0F * y * y)); // ACN 15 = sqrt(35/8) * X * (X*X - 3*Y*Y)

    if (spread > 0.0F)
    {
        // Implement the spread by using a spherical source that subtends the
        // angle spread. See:
        // http://www.ppsloan.org/publications/StupidSH36.pdf - Appendix A3
        //
        // When adjusted for N3D normalization instead of SN3D, these
        // calculations are:
        //
        // ZH0 = -sqrt(pi) * (-1+ca);
        // ZH1 =  0.5*sqrt(pi) * sa*sa;
        // ZH2 = -0.5*sqrt(pi) * ca*(-1+ca)*(ca+1);
        // ZH3 = -0.125*sqrt(pi) * (-1+ca)*(ca+1)*(5*ca*ca - 1);
        // ZH4 = -0.125*sqrt(pi) * ca*(-1+ca)*(ca+1)*(7*ca*ca - 3);
        // ZH5 = -0.0625*sqrt(pi) * (-1+ca)*(ca+1)*(21*ca*ca*ca*ca - 14*ca*ca + 1);
        //
        // The gain of the source is compensated for size, so that the
        // loundness doesn't depend on the spread. Thus:
        //
        // ZH0 = 1.0f;
        // ZH1 = 0.5f * (ca+1.0f);
        // ZH2 = 0.5f * (ca+1.0f)*ca;
        // ZH3 = 0.125f * (ca+1.0f)*(5.0f*ca*ca - 1.0f);
        // ZH4 = 0.125f * (ca+1.0f)*(7.0f*ca*ca - 3.0f)*ca;
        // ZH5 = 0.0625f * (ca+1.0f)*(21.0f*ca*ca*ca*ca - 14.0f*ca*ca + 1.0f);

        const auto ca = std::cos(spread * 0.5F);

        // Increase the source volume by up to +3dB for a full spread.
        const auto scale = std::sqrt(1.0F + (spread / tau));

        const auto zh0_norm = scale;
        const auto zh1_norm = 0.5F * (ca + 1.0F) * scale;
        const auto zh2_norm = 0.5F * (ca + 1.0F) * ca * scale;
        const auto zh3_norm = 0.125F * (ca + 1.0F) * ((5.0F * ca * ca) - 1.0F) * scale;

        // Zeroth-order
        coeffs[0] *= zh0_norm;

        // First-order
        coeffs[1] *= zh1_norm;
        coeffs[2] *= zh1_norm;
        coeffs[3] *= zh1_norm;

        // Second-order
        coeffs[4] *= zh2_norm;
        coeffs[5] *= zh2_norm;
        coeffs[6] *= zh2_norm;
        coeffs[7] *= zh2_norm;
        coeffs[8] *= zh2_norm;

        // Third-order
        coeffs[9] *= zh3_norm;
        coeffs[10] *= zh3_norm;
        coeffs[11] *= zh3_norm;
        coeffs[12] *= zh3_norm;
        coeffs[13] *= zh3_norm;
        coeffs[14] *= zh3_norm;
        coeffs[15] *= zh3_norm;
    }
}

void calc_angle_coeffs(
    const float azimuth,
    const float elevation,
    const float spread,
    float coeffs[max_ambi_coeffs])
{
    float dir[3] = {
        std::sin(azimuth) * std::cos(elevation),
        std::sin(elevation),
        -std::cos(azimuth) * std::cos(elevation)
    };

    calc_direction_coeffs(dir, spread, coeffs);
}

void compute_ambient_gains_mc(
    const ChannelConfig* channel_coeffs,
    const int num_channels,
    const float in_gain,
    float gains[max_output_channels])
{
    for (int i = 0; i < max_output_channels; ++i)
    {
        if (i < num_channels)
        {
            gains[i] = channel_coeffs[i][0] * 1.414213562F * in_gain;
        }
        else
        {
            gains[i] = 0.0F;
        }
    }
}

void compute_ambient_gains_bf(
    const BFChannelConfig* channel_map,
    const int num_channels,
    const float in_gain,
    float gains[max_output_channels])
{
    auto gain = 0.0F;

    for (int i = 0; i < num_channels; ++i)
    {
        if (channel_map[i].index == 0)
        {
            gain += channel_map[i].scale;
        }
    }

    gains[0] = gain * 1.414213562F * in_gain;

    for (int i = 1; i < max_output_channels; i++)
    {
        gains[i] = 0.0F;
    }
}

void compute_panning_gains(
    const ALCdevice* device,
    const float* const coeffs,
    const float in_gain,
    float* const out_gains)
{
    const auto& dry = device->dry;

    if (dry.coeff_count > 0)
    {
        compute_panning_gains_mc(dry.ambi.coeffs.data(), dry.num_channels, dry.coeff_count, coeffs, in_gain, out_gains);
    }
    else
    {
        compute_panning_gains_bf(dry.ambi.map.data(), dry.num_channels, coeffs, in_gain, out_gains);
    }
}

void compute_panning_gains_mc(
    const ChannelConfig* channel_coeffs,
    const int num_channels,
    const int num_coeffs,
    const float coeffs[max_ambi_coeffs],
    const float in_gain,
    float gains[max_output_channels])
{
    for (int i = 0; i < max_output_channels; ++i)
    {
        if (i < num_channels)
        {
            auto gain = 0.0F;

            for (int j = 0; j < num_coeffs; ++j)
            {
                gain += channel_coeffs[i][j] * coeffs[j];
            }

            gains[i] = clamp(gain, 0.0F, 1.0F) * in_gain;
        }
        else
        {
            gains[i] = 0.0F;
        }
    }
}

void compute_panning_gains_bf(
    const BFChannelConfig* channel_map,
    const int num_channels,
    const float coeffs[max_ambi_coeffs],
    const float in_gain,
    float gains[max_output_channels])
{
    for (int i = 0; i < max_output_channels; ++i)
    {
        if (i < num_channels)
        {
            gains[i] = channel_map[i].scale * coeffs[channel_map[i].index] * in_gain;
        }
        else
        {
            gains[i] = 0.0F;
        }
    }
}

void compute_first_order_gains(
    const ALCdevice* device,
    const float* const matrix,
    const float in_gain,
    float* const out_gains)
{
    const auto& foa_out = device->foa_out;

    if (foa_out.coeff_count > 0)
    {
        compute_first_order_gains_mc(foa_out.ambi.coeffs.data(), foa_out.num_channels, matrix, in_gain, out_gains);
    }
    else
    {
        compute_first_order_gains_bf(foa_out.ambi.map.data(), foa_out.num_channels, matrix, in_gain, out_gains);
    }
}

void compute_first_order_gains_mc(
    const ChannelConfig* channel_coeffs,
    int num_channels,
    const float mtx[4],
    float in_gain,
    float gains[max_output_channels])
{
    for (int i = 0; i < num_channels; ++i)
    {
        if (i < num_channels)
        {
            auto gain = 0.0F;

            for (int j = 0; j < 4; ++j)
            {
                gain += channel_coeffs[i][j] * mtx[j];
            }

            gains[i] = clamp(gain, 0.0F, 1.0F) * in_gain;
        }
        else
        {
            gains[i] = 0.0F;
        }
    }
}

void compute_first_order_gains_bf(
    const BFChannelConfig* channel_map,
    const int num_channels,
    const float mtx[4],
    const float in_gain,
    float gains[max_output_channels])
{
    for (int i = 0; i < max_output_channels; ++i)
    {
        if (i < num_channels)
        {
            gains[i] = channel_map[i].scale * mtx[channel_map[i].index] * in_gain;
        }
        else
        {
            gains[i] = 0.0F;
        }
    }
}

struct ChannelMap
{
    Channel name;
    ChannelConfig config;
}; // ChannelMap

static void set_channel_map(
    const Channel* device_channels,
    ChannelConfig* ambi_coeffs,
    const ChannelMap* channel_map,
    const int count,
    int* out_count)
{
    int i;

    for (i = 0; i < max_output_channels && device_channels[i] != InvalidChannel; ++i)
    {
        if (device_channels[i] == LFE)
        {
            for (int j = 0; j < max_ambi_coeffs; ++j)
            {
                ambi_coeffs[i][j] = 0.0F;
            }

            continue;
        }

        for (int j = 0; j < count; ++j)
        {
            if (device_channels[i] != channel_map[j].name)
            {
                continue;
            }

            for (int k = 0; k < max_ambi_coeffs; ++k)
            {
                ambi_coeffs[i][k] = channel_map[j].config[k];
            }

            break;
        }
    }

    *out_count = i;
}

static const ChannelMap mono_cfg[1] = {
    {FrontCenter, {1.0F}},
};

static const ChannelMap stereo_cfg[2] = {
    {FrontLeft, {5.00000000E-1F, 2.88675135E-1F, 0.0F, 1.19573156E-1F}},
    {FrontRight, {5.00000000E-1F, -2.88675135E-1F, 0.0F, 1.19573156E-1F}},
};

static const ChannelMap quad_cfg[4] = {
    {BackLeft, {3.53553391E-1F, 2.04124145E-1F, 0.0F, -2.04124145E-1F}},
    {FrontLeft, {3.53553391E-1F, 2.04124145E-1F, 0.0F, 2.04124145E-1F}},
    {FrontRight, {3.53553391E-1F, -2.04124145E-1F, 0.0F, 2.04124145E-1F}},
    {BackRight, {3.53553391E-1F, -2.04124145E-1F, 0.0F, -2.04124145E-1F}},
};

static const ChannelMap x5_1_side_cfg[5] = {
    {SideLeft, {3.33001372E-1F, 1.89085671E-1F, 0.0F, -2.00041334E-1F, -2.12309737E-2F, 0.0F, 0.0F, 0.0F, -1.14573483E-2F}},
    {FrontLeft, {1.47751298E-1F, 1.28994110E-1F, 0.0F, 1.15190495E-1F, 7.44949143E-2F, 0.0F, 0.0F, 0.0F, -6.47739980E-3F}},
    {FrontCenter, {7.73595729E-2F, 0.00000000E+0F, 0.0F, 9.71390298E-2F, 0.00000000E+0F, 0.0F, 0.0F, 0.0F, 5.18625335E-2F}},
    {FrontRight, {1.47751298E-1F, -1.28994110E-1F, 0.0F, 1.15190495E-1F, -7.44949143E-2F, 0.0F, 0.0F, 0.0F, -6.47739980E-3F}},
    {SideRight, {3.33001372E-1F, -1.89085671E-1F, 0.0F, -2.00041334E-1F, 2.12309737E-2F, 0.0F, 0.0F, 0.0F, -1.14573483E-2F}},
};

static const ChannelMap x5_1_rear_cfg[5] = {
    {BackLeft, {3.33001372E-1F, 1.89085671E-1F, 0.0F, -2.00041334E-1F, -2.12309737E-2F, 0.0F, 0.0F, 0.0F, -1.14573483E-2F}},
    {FrontLeft, {1.47751298E-1F, 1.28994110E-1F, 0.0F, 1.15190495E-1F, 7.44949143E-2F, 0.0F, 0.0F, 0.0F, -6.47739980E-3F}},
    {FrontCenter, {7.73595729E-2F, 0.00000000E+0F, 0.0F, 9.71390298E-2F, 0.00000000E+0F, 0.0F, 0.0F, 0.0F, 5.18625335E-2F}},
    {FrontRight, {1.47751298E-1F, -1.28994110E-1F, 0.0F, 1.15190495E-1F, -7.44949143E-2F, 0.0F, 0.0F, 0.0F, -6.47739980E-3F}},
    {BackRight, {3.33001372E-1F, -1.89085671E-1F, 0.0F, -2.00041334E-1F, 2.12309737E-2F, 0.0F, 0.0F, 0.0F, -1.14573483E-2F}},
};

static const ChannelMap x6_1_cfg[6] = {
    {SideLeft, {2.04462744E-1F, 2.17178497E-1F, 0.0F, -4.39990188E-2F, -2.60787329E-2F, 0.0F, 0.0F, 0.0F, -6.87238843E-2F}},
    {FrontLeft, {1.18130342E-1F, 9.34633906E-2F, 0.0F, 1.08553749E-1F, 6.80658795E-2F, 0.0F, 0.0F, 0.0F, 1.08999485E-2F}},
    {FrontCenter, {7.73595729E-2F, 0.00000000E+0F, 0.0F, 9.71390298E-2F, 0.00000000E+0F, 0.0F, 0.0F, 0.0F, 5.18625335E-2F}},
    {FrontRight, {1.18130342E-1F, -9.34633906E-2F, 0.0F, 1.08553749E-1F, -6.80658795E-2F, 0.0F, 0.0F, 0.0F, 1.08999485E-2F}},
    {SideRight, {2.04462744E-1F, -2.17178497E-1F, 0.0F, -4.39990188E-2F, 2.60787329E-2F, 0.0F, 0.0F, 0.0F, -6.87238843E-2F}},
    {BackCenter, {2.50001688E-1F, 0.00000000E+0F, 0.0F, -2.50000094E-1F, 0.00000000E+0F, 0.0F, 0.0F, 0.0F, 6.05133395E-2F}},
};

static const ChannelMap x7_1_cfg[6] = {
    {BackLeft, {2.04124145E-1F, 1.08880247E-1F, 0.0F, -1.88586120E-1F, -1.29099444E-1F, 0.0F, 0.0F, 0.0F, 7.45355993E-2F, 3.73460789E-2F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.00000000E+0F}},
    {SideLeft, {2.04124145E-1F, 2.17760495E-1F, 0.0F, 0.00000000E+0F, 0.00000000E+0F, 0.0F, 0.0F, 0.0F, -1.49071198E-1F, -3.73460789E-2F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.00000000E+0F}},
    {FrontLeft, {2.04124145E-1F, 1.08880247E-1F, 0.0F, 1.88586120E-1F, 1.29099444E-1F, 0.0F, 0.0F, 0.0F, 7.45355993E-2F, 3.73460789E-2F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.00000000E+0F}},
    {FrontRight, {2.04124145E-1F, -1.08880247E-1F, 0.0F, 1.88586120E-1F, -1.29099444E-1F, 0.0F, 0.0F, 0.0F, 7.45355993E-2F, -3.73460789E-2F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.00000000E+0F}},
    {SideRight, {2.04124145E-1F, -2.17760495E-1F, 0.0F, 0.00000000E+0F, 0.00000000E+0F, 0.0F, 0.0F, 0.0F, -1.49071198E-1F, 3.73460789E-2F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.00000000E+0F}},
    {BackRight, {2.04124145E-1F, -1.08880247E-1F, 0.0F, -1.88586120E-1F, 1.29099444E-1F, 0.0F, 0.0F, 0.0F, 7.45355993E-2F, -3.73460789E-2F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.00000000E+0F}},
};

static void init_panning(
    ALCdevice* device)
{
    const ChannelMap *channel_map = nullptr;
    auto coeff_count = 0;
    auto count = 0;

    switch (device->fmt_chans)
    {
    case DevFmtMono:
        count = count_of(mono_cfg);
        channel_map = mono_cfg;
        coeff_count = 1;
        break;

    case DevFmtStereo:
        count = count_of(stereo_cfg);
        channel_map = stereo_cfg;
        coeff_count = 4;
        break;

    case DevFmtQuad:
        count = count_of(quad_cfg);
        channel_map = quad_cfg;
        coeff_count = 4;
        break;

    case DevFmtX51:
        count = count_of(x5_1_side_cfg);
        channel_map = x5_1_side_cfg;
        coeff_count = 9;
        break;

    case DevFmtX51Rear:
        count = count_of(x5_1_rear_cfg);
        channel_map = x5_1_rear_cfg;
        coeff_count = 9;
        break;

    case DevFmtX61:
        count = count_of(x6_1_cfg);
        channel_map = x6_1_cfg;
        coeff_count = 9;
        break;

    case DevFmtX71:
        count = count_of(x7_1_cfg);
        channel_map = x7_1_cfg;
        coeff_count = 16;
        break;
    }

    set_channel_map(
        device->real_out.channel_name.data(),
        device->dry.ambi.coeffs.data(),
        channel_map,
        count,
        &device->dry.num_channels);

    device->dry.coeff_count = coeff_count;

    device->foa_out.ambi.reset();

    for (int i = 0; i < device->dry.num_channels; ++i)
    {
        device->foa_out.ambi.coeffs[i][0] = device->dry.ambi.coeffs[i][0];

        for (int j = 1; j < 4; ++j)
        {
            device->foa_out.ambi.coeffs[i][j] = device->dry.ambi.coeffs[i][j];
        }
    }

    device->foa_out.coeff_count = 4;
    device->foa_out.num_channels = 0;
    device->real_out.num_channels = 0;
}

void alu_init_renderer(
    ALCdevice* device)
{
    device->dry.ambi.reset();
    device->dry.coeff_count = 0;
    device->dry.num_channels = 0;

    for (int i = 0; i < (max_ambi_order + 1); ++i)
    {
        device->dry.num_channels_per_order[i] = 0;
    }

    set_default_wfx_channel_order(device);

    if(device->fmt_chans != DevFmtStereo)
    {
        init_panning(device);
        return;
    }

    init_panning(device);
}

void alu_init_effect_panning(
    EffectSlot* slot)
{
    for (int i = 0; i < max_effect_channels; ++i)
    {
        slot->channel_map_[i].reset();
    }

    slot->channel_count_ = 0;

    for (int i = 0; i < max_effect_channels; ++i)
    {
        slot->channel_map_[i].scale = 1.0F;
        slot->channel_map_[i].index = i;

        slot->channel_count_ += 1;
    }
}
