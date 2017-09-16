#ifndef OALSFXPP_INCLUDED
#define OALSFXPP_INCLUDED


#include "alMain.h"


class ApiImpl
{
public:
    ALCdevice device_;
    ALsource source_;
    Effect effect_;
    EffectSlot effect_slot_;


    ApiImpl();

    bool initialize(
        const ChannelFormat channel_format,
        const int sampling_rate);

    void uninitialize();


    void mix_source(
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

    void alu_mix_data(
        void* dst_buffer,
        const int sample_count,
        const float* src_samples);


private:
    struct ChannelMap
    {
        ChannelId channel_id;
        float angle;
        float elevation;
    }; // ChannelMap

    static constexpr ChannelMap mono_map[1] = {
        {ChannelId::front_center, 0.0F, 0.0F}
    };

    static constexpr ChannelMap stereo_map[2] = {
        {ChannelId::front_left, Math::deg_to_rad(-30.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_right, Math::deg_to_rad(30.0F), Math::deg_to_rad(0.0F)}
    };

    static constexpr ChannelMap quad_map[4] = {
        {ChannelId::front_left, Math::deg_to_rad(-45.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_right, Math::deg_to_rad(45.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::back_left, Math::deg_to_rad(-135.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::back_right, Math::deg_to_rad(135.0F), Math::deg_to_rad(0.0F)}
    };

    static constexpr ChannelMap x5_1_map[6] = {
        {ChannelId::front_left, Math::deg_to_rad(-30.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_right, Math::deg_to_rad(30.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_center, Math::deg_to_rad(0.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::lfe, 0.0F, 0.0F},
        {ChannelId::side_left, Math::deg_to_rad(-110.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::side_right, Math::deg_to_rad(110.0F), Math::deg_to_rad(0.0F)}
    };

    static constexpr ChannelMap x6_1_map[7] = {
        {ChannelId::front_left, Math::deg_to_rad(-30.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_right, Math::deg_to_rad(30.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_center, Math::deg_to_rad(0.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::lfe, 0.0F, 0.0F},
        {ChannelId::back_center, Math::deg_to_rad(180.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::side_left, Math::deg_to_rad(-90.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::side_right, Math::deg_to_rad(90.0F), Math::deg_to_rad(0.0F)}
    };

    static constexpr ChannelMap x7_1_map[8] = {
        {ChannelId::front_left, Math::deg_to_rad(-30.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_right, Math::deg_to_rad(30.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::front_center, Math::deg_to_rad(0.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::lfe, 0.0F, 0.0F},
        {ChannelId::back_left, Math::deg_to_rad(-150.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::back_right, Math::deg_to_rad(150.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::side_left, Math::deg_to_rad(-90.0F), Math::deg_to_rad(0.0F)},
        {ChannelId::side_right, Math::deg_to_rad(90.0F), Math::deg_to_rad(0.0F)}
    };


    static const float* apply_filters(
        FilterState* lp_filter,
        FilterState* hp_filter,
        float* dst_samples,
        const float* src_samples,
        const int sample_count,
        const ActiveFilters filter_type);

    bool calc_effect_slot_params(
        EffectSlot& effect_slot);

    void calc_panning_and_filters(
        const float distance,
        const float* dir,
        const float spread,
        const float dry_gain,
        const float dry_gain_hf,
        const float dry_gain_lf,
        const float wet_gain,
        const float wet_gain_lf,
        const float wet_gain_hf);

    void calc_non_attn_source_params();

    void update_context_sources();

    static void write_f32(
        const SampleBuffers* src_buffers,
        void* dst_buffer,
        const int offset,
        const int sample_count,
        const int channel_count);
}; // ApiImpl


#endif // OALSFXPP_INCLUDED
