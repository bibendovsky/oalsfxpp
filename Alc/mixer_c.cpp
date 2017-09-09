#include <float.h>
#include "config.h"
#include "alu.h"


void al_filter_state_process_c(FilterState* filter, float* dst, const float* src, int num_samples)
{
    int i;
    if(num_samples > 1)
    {
        dst[0] = filter->b0 * src[0] +
                 filter->b1 * filter->x[0] +
                 filter->b2 * filter->x[1] -
                 filter->a1 * filter->y[0] -
                 filter->a2 * filter->y[1];
        dst[1] = filter->b0 * src[1] +
                 filter->b1 * src[0] +
                 filter->b2 * filter->x[0] -
                 filter->a1 * dst[0] -
                 filter->a2 * filter->y[0];
        for(i = 2;i < num_samples;i++)
            dst[i] = filter->b0 * src[i] +
                     filter->b1 * src[i-1] +
                     filter->b2 * src[i-2] -
                     filter->a1 * dst[i-1] -
                     filter->a2 * dst[i-2];
        filter->x[0] = src[i-1];
        filter->x[1] = src[i-2];
        filter->y[0] = dst[i-1];
        filter->y[1] = dst[i-2];
    }
    else if(num_samples == 1)
    {
        dst[0] = filter->b0 * src[0] +
                 filter->b1 * filter->x[0] +
                 filter->b2 * filter->x[1] -
                 filter->a1 * filter->y[0] -
                 filter->a2 * filter->y[1];
        filter->x[1] = filter->x[0];
        filter->x[0] = src[0];
        filter->y[1] = filter->y[0];
        filter->y[0] = dst[0];
    }
}

void mix_c(
    const float* data,
    int out_chans,
    SampleBuffers& out_buffer,
    float* current_gains,
    const float* target_gains,
    int counter,
    int out_pos,
    int buffer_size)
{
    float gain, delta, step;
    int c;

    delta = (counter > 0) ? 1.0f/(float)counter : 0.0f;

    for(c = 0;c < out_chans;c++)
    {
        int pos = 0;
        gain = current_gains[c];
        step = (target_gains[c] - gain) * delta;
        if(fabsf(step) > FLT_EPSILON)
        {
            int minsize = std::min(buffer_size, counter);
            for(;pos < minsize;pos++)
            {
                out_buffer[c][out_pos+pos] += data[pos]*gain;
                gain += step;
            }
            if(pos == counter)
                gain = target_gains[c];
            current_gains[c] = gain;
        }

        if(!(fabsf(gain) > silence_threshold_gain))
            continue;
        for(;pos < buffer_size;pos++)
            out_buffer[c][out_pos+pos] += data[pos]*gain;
    }
}

/* Basically the inverse of the above. Rather than one input going to multiple
 * outputs (each with its own gain), it's multiple inputs (each with its own
 * gain) going to one output. This applies one row (vs one column) of a matrix
 * transform. And as the matrices are more or less static once set up, no
 * stepping is necessary.
 */
void mix_row_c(
    float* out_buffer,
    const float* gains,
    const SampleBuffers& data,
    int in_chans,
    int in_pos,
    int buffer_size)
{
    int c, i;

    for(c = 0;c < in_chans;c++)
    {
        float gain = gains[c];
        if(!(fabsf(gain) > silence_threshold_gain))
            continue;

        for(i = 0;i < buffer_size;i++)
            out_buffer[i] += data[c][in_pos+i] * gain;
    }
}
