#ifndef MIXER_DEFS_H
#define MIXER_DEFS_H

#include "AL/alc.h"
#include "AL/al.h"
#include "alMain.h"
#include "alu.h"


/* C mixers */
void Mix_C(const ALfloat *data, ALsizei OutChans, ALfloat (*OutBuffer)[BUFFERSIZE],
           ALfloat *CurrentGains, const ALfloat *TargetGains, ALsizei Counter, ALsizei OutPos,
           ALsizei BufferSize);
void MixRow_C(ALfloat *OutBuffer, const ALfloat *Gains,
              const ALfloat (*data)[BUFFERSIZE], ALsizei InChans,
              ALsizei InPos, ALsizei BufferSize);


#endif /* MIXER_DEFS_H */
