#ifndef _AL_BUFFER_H_
#define _AL_BUFFER_H_


#include "alMain.h"


// User formats
enum UserFmtChannels
{
    UserFmtMono = AL_MONO_SOFT,
    UserFmtStereo = AL_STEREO_SOFT
}; // UserFmtChannels


// Storable formats
enum FmtChannels
{
    FmtMono = UserFmtMono,
    FmtStereo = UserFmtStereo
}; // FmtChannels


constexpr auto max_input_channels = 8;


#endif
