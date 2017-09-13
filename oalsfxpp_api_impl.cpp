#include "oalsfxpp_api_impl.h"
#include <new>
#include "alSource.h"


ALCdevice* g_device = nullptr;
ALsource* g_source = nullptr;
Effect* g_effect = nullptr;
EffectSlot* g_effect_slot = nullptr;


bool ApiImpl::initialize(
    const int channel_count,
    const int sampling_rate)
{
    g_device = new (std::nothrow) ALCdevice{};
    g_source = new (std::nothrow) ALsource{};
    g_effect = new (std::nothrow) Effect{};
    g_effect_slot = new (std::nothrow) EffectSlot{};

    if (!g_device || !g_source || !g_effect || !g_effect_slot)
    {
        uninitialize();
        return false;
    }

    g_device->initialize(channel_count, sampling_rate);
    g_effect->initialize();

    auto effect_state = g_effect_slot->effect_state_.get();
    effect_state->dst_buffers_ = &g_device->sample_buffers_;
    effect_state->dst_channel_count_ = channel_count;
    effect_state->update_device(g_device);
    g_effect_slot->is_props_updated_ = true;

    auto source = g_source;

    for (int i = 0; i < channel_count; ++i)
    {
        source->direct_.channels_[i].reset();
        source->aux_.channels_[i].reset();
    }

    return true;
}

void ApiImpl::uninitialize()
{
    delete g_effect;
    delete g_effect_slot;
    delete g_source;
    delete g_device;
}
