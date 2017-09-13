#ifndef OALSFXPP_API_IMPL_INCLUDED
#define OALSFXPP_API_IMPL_INCLUDED


#include "alMain.h"


class ApiImpl
{
public:
    static bool initialize(
        const int channel_count,
        const int sampling_rate);

    static void uninitialize();
}; // ApiImpl


#endif // OALSFXPP_API_IMPL_INCLUDED
