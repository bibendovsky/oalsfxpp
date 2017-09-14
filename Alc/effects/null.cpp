#include "alAuxEffectSlot.h"


class NullEffectState :
    public EffectState
{
public:
    NullEffectState()
        :
        EffectState{}
    {
    }

    virtual ~NullEffectState()
    {
    }


protected:
    void do_construct() final;

    void do_destruct() final;

    void do_update_device(
        ALCdevice* device) final;

    void do_update(
        ALCdevice* device,
        const EffectSlot* slot,
        const EffectProps* props) final;

    void do_process(
        int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final;
}; // NullEffectState


void NullEffectState::do_construct()
{
}

void NullEffectState::do_destruct()
{
}

void NullEffectState::do_update_device(
    ALCdevice* device)
{
    static_cast<void>(device);
}

void NullEffectState::do_update(
    ALCdevice* device,
    const EffectSlot* slot,
    const EffectProps* props)
{
    static_cast<void>(device);
    static_cast<void>(slot);
    static_cast<void>(props);
}

void NullEffectState::do_process(
    int sample_count,
    const SampleBuffers& src_samples,
    SampleBuffers& dst_samples,
    const int channel_count)
{
    static_cast<void>(sample_count);
    static_cast<void>(src_samples);
    static_cast<void>(dst_samples);
    static_cast<void>(channel_count);
}

EffectState* EffectStateFactory::create_null()
{
    return create<NullEffectState>();
}
