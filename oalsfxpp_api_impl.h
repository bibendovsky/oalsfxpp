#ifndef OALSFXPP_API_IMPL_INCLUDED
#define OALSFXPP_API_IMPL_INCLUDED


#include "alMain.h"


class ApiImpl
{
public:
    static bool initialize(
        const ChannelFormat channel_format,
        const int sampling_rate);

    static void uninitialize();
}; // ApiImpl


#endif // OALSFXPP_API_IMPL_INCLUDED
