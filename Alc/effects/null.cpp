#include "config.h"
#include "alAuxEffectSlot.h"


class NullEffect :
    public IEffect
{
public:
    NullEffect()
        :
        IEffect{}
    {
    }

    virtual ~NullEffect()
    {
    }


protected:
    void do_construct() final;

    void do_destruct() final;

    void do_update_device(
        ALCdevice* device) final;

    void do_update(
        ALCdevice* device,
        const struct ALeffectslot* slot,
        const union ALeffectProps *props) final;

    void do_process(
        int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final;
}; // NullEffect


void NullEffect::do_construct()
{
}

void NullEffect::do_destruct()
{
}

void NullEffect::do_update_device(
    ALCdevice* device)
{
    static_cast<void>(device);
}

void NullEffect::do_update(
    ALCdevice* device,
    const struct ALeffectslot* slot,
    const union ALeffectProps *props)
{
    static_cast<void>(device);
    static_cast<void>(slot);
    static_cast<void>(props);
}

void NullEffect::do_process(
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

IEffect* create_null_effect()
{
    return create_effect<NullEffect>();
}
