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


    static void mix_source(
        ALsource* source,
        ALCdevice* device,
        const int sample_count);

    static void mix_row_c(
        float* dst_buffer,
        const float* gains,
        const SampleBuffers& src_buffers,
        const int channel_count,
        const int src_position,
        const int buffer_size);

    static void mix_c(
        const float* data,
        const int channel_count,
        SampleBuffers& dst_buffers,
        float* current_gains,
        const float* target_gains,
        const int counter,
        const int dst_position,
        const int buffer_size);


private:
    static const float* apply_filters(
        FilterState* lp_filter,
        FilterState* hp_filter,
        float* dst_samples,
        const float* src_samples,
        const int sample_count,
        const ActiveFilters filter_type);
}; // ApiImpl


#endif // OALSFXPP_API_IMPL_INCLUDED
