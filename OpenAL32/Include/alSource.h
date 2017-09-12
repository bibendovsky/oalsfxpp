#ifndef _AL_SOURCE_H_
#define _AL_SOURCE_H_


#include "alu.h"


struct ALsource
{
    struct Send
    {
        struct Channel
        {
            using Gains = std::array<float, max_output_channels>;

            FilterState low_pass_;
            FilterState high_pass_;
            Gains current_gains_;
            Gains target_gains_;


            void reset();
        }; // Channel

        using Channels = std::array<Channel, max_input_channels>;


        float gain_;
        float gain_hf_;
        float hf_reference_;
        float gain_lf_;
        float lf_reference_;

        ActiveFilters filter_type_;
        Channels channels_;
        SampleBuffers* buffers_;
        int channel_count_;
    }; // Send


    Send direct_;
    Send aux_;
}; // ALsource


#endif
