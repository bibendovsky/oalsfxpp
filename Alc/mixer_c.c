#include "config.h"

#include "alMain.h"
#include "alu.h"


void ALfilterState_processC(ALfilterState *filter, ALfloat *restrict dst, const ALfloat *restrict src, ALsizei numsamples)
{
    ALsizei i;
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


void Mix_C(const ALfloat *data, ALsizei OutChans, ALfloat (*restrict OutBuffer)[BUFFERSIZE],
           ALfloat *CurrentGains, const ALfloat *TargetGains, ALsizei Counter, ALsizei OutPos,
           ALsizei BufferSize)
{
    ALfloat gain, delta, step;
    ALsizei c;

    delta = (Counter > 0) ? 1.0f/(ALfloat)Counter : 0.0f;

    for(c = 0;c < OutChans;c++)
    {
        ALsizei pos = 0;
        gain = CurrentGains[c];
        step = (TargetGains[c] - gain) * delta;
        if(fabsf(step) > FLT_EPSILON)
        {
            ALsizei minsize = mini(BufferSize, Counter);
            for(;pos < minsize;pos++)
            {
                OutBuffer[c][OutPos+pos] += data[pos]*gain;
                gain += step;
            }
            if(pos == Counter)
                gain = TargetGains[c];
            CurrentGains[c] = gain;
        }

        if(!(fabsf(gain) > GAIN_SILENCE_THRESHOLD))
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
void MixRow_C(ALfloat *OutBuffer, const ALfloat *Gains, const ALfloat (*restrict data)[BUFFERSIZE], ALsizei InChans, ALsizei InPos, ALsizei BufferSize)
{
    ALsizei c, i;

    for(c = 0;c < InChans;c++)
    {
        ALfloat gain = Gains[c];
        if(!(fabsf(gain) > GAIN_SILENCE_THRESHOLD))
            continue;

        for(i = 0;i < BufferSize;i++)
            OutBuffer[i] += data[c][InPos+i] * gain;
    }
}
