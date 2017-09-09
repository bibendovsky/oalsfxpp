/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2007 by authors.
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
#include "alFilter.h"


void FilterState::reset()
{
    x[0] = 0.0F;
    x[1] = 0.0F;

    y[0] = 0.0F;
    y[1] = 0.0F;

    b0 = 0.0F;
    b1 = 0.0F;
    b2 = 0.0F;

    a1 = 0.0F;
    a2 = 0.0F;
}


void al_filter_state_clear(
    FilterState* filter)
{
    filter->x[0] = 0.0F;
    filter->x[1] = 0.0F;
    filter->y[0] = 0.0F;
    filter->y[1] = 0.0F;
}

void al_filter_state_copy_params(
    FilterState* dst,
    const FilterState* src)
{
    dst->b0 = src->b0;
    dst->b1 = src->b1;
    dst->b2 = src->b2;
    dst->a1 = src->a1;
    dst->a2 = src->a2;
}

void al_filter_state_process_pass_through(
    FilterState* filter,
    const float* src,
    const int num_samples)
{
    if (num_samples >= 2)
    {
        filter->x[1] = src[num_samples - 2];
        filter->x[0] = src[num_samples - 1];
        filter->y[1] = src[num_samples - 2];
        filter->y[0] = src[num_samples - 1];
    }
    else if (num_samples == 1)
    {
        filter->x[1] = filter->x[0];
        filter->x[0] = src[0];
        filter->y[1] = filter->y[0];
        filter->y[0] = src[0];
    }
}

float calc_rcp_q_from_slope(
    const float gain,
    const float slope)
{
    return std::sqrt((gain + (1.0F / gain)) * ((1.0F / slope) - 1.0f) + 2.0F);
}

float calc_rcp_q_from_bandwidth(
    const float freq_mult,
    const float bandwidth)
{
    const auto w0 = tau * freq_mult;
    return 2.0F * std::sinh(std::log(2.0F) / 2.0F * bandwidth * w0 / std::sin(w0));
}

void al_filter_state_set_params(
    FilterState* filter,
    const FilterType type,
    const float gain,
    const float freq_mult,
    const float rcp_q)
{
    // Limit gain to -100dB
    assert(gain > 0.00001F);


    const auto w0 = tau * freq_mult;
    const auto sin_w0 = std::sin(w0);
    const auto cos_w0 = std::cos(w0);
    const auto alpha = sin_w0 / 2.0F * rcp_q;

    auto sqrt_gain_alpha_2 = 0.0F;

    float a[3];
    float b[3];


    // Calculate filter coefficients depending on filter type
    switch (type)
    {
    case FilterType::high_shelf:
        sqrt_gain_alpha_2 = 2.0F * std::sqrt(gain) * alpha;

        b[0] = gain * ((gain + 1.0F) + ((gain - 1.0F) * cos_w0) + sqrt_gain_alpha_2);
        b[1] = -2.0F * gain * ((gain - 1.0F) + ((gain + 1.0F) * cos_w0));
        b[2] = gain * ((gain + 1.0F) + ((gain - 1.0F) * cos_w0) - sqrt_gain_alpha_2);

        a[0] = (gain + 1.0F) - ((gain - 1.0F) * cos_w0) + sqrt_gain_alpha_2;
        a[1] = 2.0F * ((gain - 1.0F) - ((gain + 1.0F) * cos_w0));
        a[2] = (gain + 1.0F) - ((gain - 1.0F) * cos_w0) - sqrt_gain_alpha_2;

        break;

    case FilterType::low_shelf:
        sqrt_gain_alpha_2 = 2.0F * std::sqrt(gain) * alpha;

        b[0] = gain * ((gain + 1.0F) - ((gain - 1.0F) * cos_w0) + sqrt_gain_alpha_2);
        b[1] = 2.0F * gain * ((gain - 1.0F) - ((gain + 1.0F) * cos_w0));
        b[2] = gain * ((gain + 1.0F) - ((gain - 1.0F) * cos_w0) - sqrt_gain_alpha_2);

        a[0] = (gain + 1.0F) + ((gain - 1.0F) * cos_w0) + sqrt_gain_alpha_2;
        a[1] = -2.0F * ((gain - 1.0F) + ((gain + 1.0F) * cos_w0));
        a[2] = (gain + 1.0F) + ((gain - 1.0F) * cos_w0) - sqrt_gain_alpha_2;

        break;

    case FilterType::peaking:
    {
        const auto sqrt_gain = std::sqrt(gain);

        b[0] = 1.0F + (alpha * sqrt_gain);
        b[1] = -2.0F * cos_w0;
        b[2] = 1.0F - (alpha * sqrt_gain);

        a[0] = 1.0F + (alpha / sqrt_gain);
        a[1] = -2.0F * cos_w0;
        a[2] = 1.0F - (alpha / sqrt_gain);

        break;
    }

    case FilterType::low_pass:
        b[0] = (1.0F - cos_w0) / 2.0F;
        b[1] = 1.0F - cos_w0;
        b[2] = (1.0F - cos_w0) / 2.0F;

        a[0] = 1.0F + alpha;
        a[1] = -2.0F * cos_w0;
        a[2] = 1.0F - alpha;

        break;

    case FilterType::high_pass:
        b[0] = (1.0F + cos_w0) / 2.0F;
        b[1] = -(1.0F + cos_w0);
        b[2] = (1.0F + cos_w0) / 2.0F;

        a[0] = 1.0F + alpha;
        a[1] = -2.0F * cos_w0;
        a[2] = 1.0F - alpha;

        break;

    case FilterType::band_pass:
        b[0] = alpha;
        b[1] = 0;
        b[2] = -alpha;

        a[0] = 1.0F + alpha;
        a[1] = -2.0F * cos_w0;
        a[2] = 1.0F - alpha;

        break;

    default:
        b[0] = 1.0F;
        b[1] = 0.0F;
        b[2] = 0.0F;

        a[0] = 1.0F;
        a[1] = 0.0F;
        a[2] = 0.0F;

        break;
    }

    filter->a1 = a[1] / a[0];
    filter->a2 = a[2] / a[0];
    filter->b0 = b[0] / a[0];
    filter->b1 = b[1] / a[0];
    filter->b2 = b[2] / a[0];
}
