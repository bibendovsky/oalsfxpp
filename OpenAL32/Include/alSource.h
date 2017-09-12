#ifndef _AL_SOURCE_H_
#define _AL_SOURCE_H_


#include "alu.h"


struct ALsource
{
    struct Base
    {
        float gain_;
        float gain_hf_;
        float hf_reference_;
        float gain_lf_;
        float lf_reference_;
    }; // Base

    struct Send : Base
    {
        EffectSlot* effect_slot_;
    }; // Send

    struct Props
    {
        Base direct_;
        Send send_;
    }; // Props

    struct State
    {
        struct Param
        {
            using Gains = std::array<float, max_output_channels>;

            FilterState low_pass_;
            FilterState high_pass_;
            Gains current_gains_;
            Gains target_gains_;


            void reset();
        }; // Param

        using Params = std::array<Param, max_input_channels>;


        ActiveFilters filter_type_;
        Params params_;
        SampleBuffers* buffers_;
        int channel_count_;
    }; // State


    Props props_;

    State direct_;
    State send_;
}; // ALsource


#endif
