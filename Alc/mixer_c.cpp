#include <float.h>
#include "config.h"
#include "alu.h"


void mix_c(
    const float* data,
    const int channel_count,
    SampleBuffers& dst_buffers,
    float* current_gains,
    const float* target_gains,
    const int counter,
    const int dst_position,
    const int buffer_size)
{
    const auto delta = ((counter > 0) ? 1.0F / static_cast<float>(counter) : 0.0F);

    for (int c = 0; c < channel_count; ++c)
    {
        auto pos = 0;
        auto gain = current_gains[c];
        const auto step = (target_gains[c] - gain) * delta;

        if (std::abs(step) > FLT_EPSILON)
        {
            const auto size = std::min(buffer_size, counter);

            for ( ; pos < size; ++pos)
            {
                dst_buffers[c][dst_position + pos] += data[pos] * gain;
                gain += step;
            }

            if (pos == counter)
            {
                gain = target_gains[c];
            }

            current_gains[c] = gain;
        }

        if (!(std::abs(gain) > silence_threshold_gain))
        {
            continue;
        }

        for ( ; pos < buffer_size; ++pos)
        {
            dst_buffers[c][dst_position + pos] += data[pos] * gain;
        }
    }
}

// Basically the inverse of the above. Rather than one input going to multiple
// outputs (each with its own gain), it's multiple inputs (each with its own
// gain) going to one output. This applies one row (vs one column) of a matrix
// transform. And as the matrices are more or less static once set up, no
// stepping is necessary.
void mix_row_c(
    float* dst_buffer,
    const float* gains,
    const SampleBuffers& src_buffers,
    const int channel_count,
    const int src_position,
    const int buffer_size)
{
    for (int c = 0; c < channel_count; ++c)
    {
        const auto gain = gains[c];

        if (!(std::abs(gain) > silence_threshold_gain))
        {
            continue;
        }

        for (int i = 0; i < buffer_size; ++i)
        {
            dst_buffer[i] += src_buffers[c][src_position + i] * gain;
        }
    }
}
