#include "alMain.h"


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
    void do_construct() final
    {
    }

    void do_destruct() final
    {
    }

    void do_update_device(
        ALCdevice& device) final
    {
        static_cast<void>(device);
    }

    void do_update(
        ALCdevice& device,
        const EffectSlot& effect_slot,
        const EffectProps& effect_props) final
    {
        static_cast<void>(device);
        static_cast<void>(effect_slot);
        static_cast<void>(effect_props);
    }

    void do_process(
        int sample_count,
        const SampleBuffers& src_samples,
        SampleBuffers& dst_samples,
        const int channel_count) final
    {
        static_cast<void>(sample_count);
        static_cast<void>(src_samples);
        static_cast<void>(dst_samples);
        static_cast<void>(channel_count);
    }
}; // NullEffectState


EffectState* EffectStateFactory::create_null()
{
    return create<NullEffectState>();
}
