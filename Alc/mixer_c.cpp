#include <float.h>
#include "config.h"
#include "alu.h"


void ALfilterState_processC(ALfilterState *filter, float *dst, const float *src, int numsamples)
{
    int i;
    if(numsamples > 1)
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
        for(i = 2;i < numsamples;i++)
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
    else if(numsamples == 1)
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


void Mix_C(const float *data, int OutChans, SampleBuffers& OutBuffer,
           float *CurrentGains, const float *TargetGains, int Counter, int OutPos,
           int BufferSize)
{
    float gain, delta, step;
    int c;

    delta = (Counter > 0) ? 1.0f/(float)Counter : 0.0f;

    for(c = 0;c < OutChans;c++)
    {
        int pos = 0;
        gain = CurrentGains[c];
        step = (TargetGains[c] - gain) * delta;
        if(fabsf(step) > FLT_EPSILON)
        {
            int minsize = std::min(BufferSize, Counter);
            for(;pos < minsize;pos++)
            {
                OutBuffer[c][OutPos+pos] += data[pos]*gain;
                gain += step;
            }
            if(pos == Counter)
                gain = TargetGains[c];
            CurrentGains[c] = gain;
        }

        if(!(fabsf(gain) > silence_threshold_gain))
            continue;
        for(;pos < BufferSize;pos++)
            OutBuffer[c][OutPos+pos] += data[pos]*gain;
    }
}

/* Basically the inverse of the above. Rather than one input going to multiple
 * outputs (each with its own gain), it's multiple inputs (each with its own
 * gain) going to one output. This applies one row (vs one column) of a matrix
 * transform. And as the matrices are more or less static once set up, no
 * stepping is necessary.
 */
void MixRow_C(float *OutBuffer, const float *Gains, const SampleBuffers& data, int InChans, int InPos, int BufferSize)
{
    int c, i;

    for(c = 0;c < InChans;c++)
    {
        float gain = Gains[c];
        if(!(fabsf(gain) > silence_threshold_gain))
            continue;

        for(i = 0;i < BufferSize;i++)
            OutBuffer[i] += data[c][InPos+i] * gain;
    }
}
