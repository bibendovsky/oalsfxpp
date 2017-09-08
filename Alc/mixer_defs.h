#ifndef MIXER_DEFS_H
#define MIXER_DEFS_H


#include "alu.h"


/* C mixers */
void mix_c(
    const float* data,
    int out_chans,
    SampleBuffers& out_buffer,
    float* current_gains,
    const float* target_gains,
    int counter,
    int out_pos,
    int buffer_size);

void mix_row_c(
    float* out_buffer,
    const float* gains,
    const SampleBuffers& data,
    int in_chans,
    int in_pos,
    int buffer_size);


#endif /* MIXER_DEFS_H */
