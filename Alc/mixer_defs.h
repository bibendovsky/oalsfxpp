#ifndef MIXER_DEFS_H
#define MIXER_DEFS_H


#include "alu.h"


/* C mixers */
void Mix_C(const float *data, int OutChans, SampleBuffers& OutBuffer,
           float *CurrentGains, const float *TargetGains, int Counter, int OutPos,
           int BufferSize);
void MixRow_C(float *OutBuffer, const float *Gains,
              const SampleBuffers& data, int InChans,
              int InPos, int BufferSize);


#endif /* MIXER_DEFS_H */
