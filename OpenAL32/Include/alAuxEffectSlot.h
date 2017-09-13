#ifndef _AL_AUXEFFECTSLOT_H_
#define _AL_AUXEFFECTSLOT_H_


#include <memory>
#include "alEffect.h"


constexpr auto max_effect_channels = 4;


struct EffectSlot
{
    using EffectStateUPtr = std::unique_ptr<EffectState, EffectStateDeleter>;


    Effect effect_;
    EffectStateUPtr effect_state_;
    bool is_props_updated_;

    int channel_count_;
    BFChannelConfig channel_map_[max_effect_channels];

    // Wet buffer configuration is ACN channel order with N3D scaling:
    // * Channel 0 is the unattenuated mono signal.
    // * Channel 1 is OpenAL -X
    // * Channel 2 is OpenAL Y
    // * Channel 3 is OpenAL -Z
    // Consequently, effects that only want to work with mono input can use
    // channel 0 by itself. Effects that want multichannel can process the
    // ambisonics signal and make a B-Format pan (ComputeFirstOrderGains) for
    // first-order device output (FOAOut).
    SampleBuffers wet_buffer_;


    EffectSlot()
        :
        effect_{},
        effect_state_{},
        is_props_updated_{},
        channel_count_{},
        channel_map_{},
        wet_buffer_{SampleBuffers::size_type{max_effect_channels}}
    {
        initialize();
    }


    EffectSlot(
        const EffectSlot& that) = delete;

    EffectSlot& operator=(
        const EffectSlot& that) = delete;

    ~EffectSlot()
    {
        uninitialize();
    }

    void initialize()
    {
        uninitialize();

        effect_.type_ = EffectType::null;
        effect_state_.reset(EffectStateFactory::create_by_type(EffectType::null));
        is_props_updated_ = true;
    }

    void uninitialize()
    {
        effect_state_.reset(nullptr);
    }

    void initialize_effect(
        ALCdevice* device)
    {
        if (effect_.type_ != device->effect_->type_)
        {
            effect_state_.reset(EffectStateFactory::create_by_type(device->effect_->type_));

            effect_state_->dst_buffers_ = &device->sample_buffers_;
            effect_state_->dst_channel_count_ = device->channel_count_;
            effect_state_->update_device(device);

            effect_.type_ = device->effect_->type_;
            effect_.props_ = device->effect_->props_;
        }
        else
        {
            effect_.props_ = device->effect_->props_;
        }

        is_props_updated_ = true;
    }
}; // EffectSlot


#endif
