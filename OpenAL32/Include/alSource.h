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


            void reset()
            {
                low_pass_.reset();
                high_pass_.reset();

                current_gains_.fill(0.0F);
                target_gains_.fill(0.0F);
            }
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


    ALsource()
    {
        initialize();
    }

    void initialize()
    {
        direct_.gain_ = 1.0F;
        direct_.gain_hf_ = 1.0F;
        direct_.hf_reference_ = FilterState::lp_frequency_reference;
        direct_.gain_lf_ = 1.0F;
        direct_.lf_reference_ = FilterState::hp_frequency_reference;
        aux_.gain_ = 1.0F;
        aux_.gain_hf_ = 1.0F;
        aux_.hf_reference_ = FilterState::lp_frequency_reference;
        aux_.gain_lf_ = 1.0F;
        aux_.lf_reference_ = FilterState::hp_frequency_reference;
    }
}; // ALsource


#endif
