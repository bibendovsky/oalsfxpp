#ifndef MIXER_DEFS_H
#define MIXER_DEFS_H


#include "alu.h"


/* C mixers */
void Mix_C(const ALfloat *data, ALsizei OutChans, SampleBuffers& OutBuffer,
           ALfloat *CurrentGains, const ALfloat *TargetGains, ALsizei Counter, ALsizei OutPos,
           ALsizei BufferSize);
void MixRow_C(ALfloat *OutBuffer, const ALfloat *Gains,
              const SampleBuffers& data, ALsizei InChans,
              ALsizei InPos, ALsizei BufferSize);


#endif /* MIXER_DEFS_H */
