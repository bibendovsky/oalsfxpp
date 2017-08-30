#ifndef _AL_SOURCE_H_
#define _AL_SOURCE_H_

#include "alu.h"

#define MAX_SENDS      1
#define DEFAULT_SENDS  1


struct ALsource;


typedef struct ALsource {
    /** Direct filter and auxiliary send info. */
    struct SourceDirect {
        ALfloat gain;
        ALfloat gain_hf;
        ALfloat hf_reference;
        ALfloat gain_lf;
        ALfloat lf_reference;
    } direct;
    struct SourceSend {
        struct ALeffectslot *slot;
        ALfloat gain;
        ALfloat gain_hf;
        ALfloat hf_reference;
        ALfloat gain_lf;
        ALfloat lf_reference;
    } *send;

    /** Source state (initial, playing, paused, or stopped) */
    ALenum state;
} ALsource;


void UpdateAllSourceProps(ALCcontext *context);


#endif
