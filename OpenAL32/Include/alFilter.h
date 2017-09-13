#ifndef _AL_FILTER_H_
#define _AL_FILTER_H_


#include <cassert>
#include "alMain.h"
#include "math_defs.h"


constexpr auto lp_frequency_reference = 5000.0F;
constexpr auto hp_frequency_reference = 250.0F;


// Filters implementation is based on the "Cookbook formulae for audio
// EQ biquad filter coefficients" by Robert Bristow-Johnson
// http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
//
// Implementation note: For the shelf filters, the specified gain is for the
// reference frequency, which is the centerpoint of the transition band. This
// better matches EFX filter design. To set the gain for the shelf itself, use
// the square root of the desired linear gain (or halve the dB gain).

enum class FilterType
{
    // EFX-style low-pass filter, specifying a gain and reference frequency.
    high_shelf,

    // EFX-style high-pass filter, specifying a gain and reference frequency.
    low_shelf,

    // Peaking filter, specifying a gain and reference frequency.
    peaking,

    // Low-pass cut-off filter, specifying a cut-off frequency.
    low_pass,

    // High-pass cut-off filter, specifying a cut-off frequency.
    high_pass,

    // Band-pass filter, specifying a center frequency.
    band_pass,
}; // FilterType


struct FilterState
{
    float x_[2]; // History of two last input samples
    float y_[2]; // History of two last output samples

    // Transfer function coefficients "b"
    float b0_;
    float b1_;
    float b2_;

    // Transfer function coefficients "a" (a0 is pre-applied)
    float a1_;
    float a2_;


    void reset()
    {
        x_[0] = 0.0F;
        x_[1] = 0.0F;

        y_[0] = 0.0F;
        y_[1] = 0.0F;

        b0_ = 0.0F;
        b1_ = 0.0F;
        b2_ = 0.0F;

        a1_ = 0.0F;
        a2_ = 0.0F;
    }

    void clear()
    {
        x_[0] = 0.0F;
        x_[1] = 0.0F;
        y_[0] = 0.0F;
        y_[1] = 0.0F;
    }

    void set_params(
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

        a1_ = a[1] / a[0];
        a2_ = a[2] / a[0];
        b0_ = b[0] / a[0];
        b1_ = b[1] / a[0];
        b2_ = b[2] / a[0];
    }

    void process(
        float* dst_samples,
        const float* src_samples,
        const int sample_count)
    {
        if (sample_count > 1)
        {
            dst_samples[0] =
                (b0_ * src_samples[0]) +
                (b1_ * x_[0]) +
                (b2_ * x_[1]) -
                (a1_ * y_[0]) -
                (a2_ * y_[1]);

            dst_samples[1] =
                (b0_ * src_samples[1]) +
                (b1_ * src_samples[0]) +
                (b2_ * x_[0]) -
                (a1_ * dst_samples[0]) -
                (a2_ * y_[0]);

            auto i = 0;

            for (i = 2; i < sample_count; ++i)
            {
                dst_samples[i] =
                    (b0_ * src_samples[i]) +
                    (b1_ * src_samples[i - 1]) +
                    (b2_ * src_samples[i - 2]) -
                    (a1_ * dst_samples[i - 1]) -
                    (a2_ * dst_samples[i - 2]);
            }

            x_[0] = src_samples[i - 1];
            x_[1] = src_samples[i - 2];
            y_[0] = dst_samples[i - 1];
            y_[1] = dst_samples[i - 2];
        }
        else if (sample_count == 1)
        {
            dst_samples[0] =
                (b0_ * src_samples[0]) +
                (b1_ * x_[0]) +
                (b2_ * x_[1]) -
                (a1_ * y_[0]) -
                (a2_ * y_[1]);

            x_[1] = x_[0];
            x_[0] = src_samples[0];
            y_[1] = y_[0];
            y_[0] = dst_samples[0];
        }
    }

    void process_pass_through(
        const float* src,
        const int num_samples)
    {
        if (num_samples >= 2)
        {
            x_[1] = src[num_samples - 2];
            x_[0] = src[num_samples - 1];
            y_[1] = src[num_samples - 2];
            y_[0] = src[num_samples - 1];
        }
        else if (num_samples == 1)
        {
            x_[1] = x_[0];
            x_[0] = src[0];
            y_[1] = y_[0];
            y_[0] = src[0];
        }
    }


    static void copy_params(
        FilterState& dst,
        const FilterState& src)
    {
        dst.b0_ = src.b0_;
        dst.b1_ = src.b1_;
        dst.b2_ = src.b2_;
        dst.a1_ = src.a1_;
        dst.a2_ = src.a2_;
    }

    // Calculates the rcpQ (i.e. 1/Q) coefficient for shelving filters, using the
    // reference gain and shelf slope parameter.
    // 0 < gain
    // 0 < slope <= 1
    static float calc_rcp_q_from_slope(
        const float gain,
        const float slope)
    {
        return std::sqrt((gain + (1.0F / gain)) * ((1.0F / slope) - 1.0f) + 2.0F);
    }

    // Calculates the rcpQ (i.e. 1/Q) coefficient for filters, using the frequency
    // multiple (i.e. ref_freq / sampling_freq) and bandwidth.
    // 0 < freq_mult < 0.5.
    static float calc_rcp_q_from_bandwidth(
        const float freq_mult,
        const float bandwidth)
    {
        const auto w0 = tau * freq_mult;
        return 2.0F * std::sinh(std::log(2.0F) / 2.0F * bandwidth * w0 / std::sin(w0));
    }
}; // FilterState


#endif
