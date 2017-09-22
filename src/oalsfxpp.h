/*
A standalone OpenAL Soft effects for C++.

Copyright (C) 2017 Boris I. Bendovsky (bibendovsky@hotmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

For a copy of the GNU General Public License see file COPYING.
*/


#ifndef OALSFXPP_INCLUDED
#define OALSFXPP_INCLUDED


#include <array>
#include <memory>


namespace oalsfxpp
{


enum class ChannelFormat
{
    none,
    mono,
    stereo,
    quad,
    five_point_one,
    five_point_one_rear,
    six_point_one,
    seven_point_one,
}; // ChannelFormat

enum class EffectType
{
    null,
    chorus,
    compressor,
    dedicated_dialog,
    dedicated_low_frequency,
    distortion,
    echo,
    equalizer,
    flanger,
    ring_modulator,
    reverb,
    eax_reverb,
}; // EffectType


union EffectProps
{
    using Pan = std::array<float, 3>;


    struct Null
    {
    }; // Null

    struct Chorus
    {
        static constexpr auto waveform_sinusoid = 0;
        static constexpr auto waveform_triangle = 1;

        static constexpr auto min_waveform = waveform_sinusoid;
        static constexpr auto max_waveform = waveform_triangle;
        static constexpr auto default_waveform = waveform_triangle;

        static constexpr auto min_phase = -180;
        static constexpr auto max_phase = 180;
        static constexpr auto default_phase = 90;

        static constexpr auto min_rate = 0.0F;
        static constexpr auto max_rate = 10.0F;
        static constexpr auto default_rate = 1.1F;

        static constexpr auto min_depth = 0.0F;
        static constexpr auto max_depth = 1.0F;
        static constexpr auto default_depth = 0.1F;

        static constexpr auto min_feedback = -1.0F;
        static constexpr auto max_feedback = 1.0F;
        static constexpr auto default_feedback = 0.25F;

        static constexpr auto min_delay = 0.0F;
        static constexpr auto max_delay = 0.016F;
        static constexpr auto default_delay = 0.016F;


        int waveform_;
        int phase_;
        float rate_;
        float depth_;
        float feedback_;
        float delay_;


        void set_defaults();

        void normalize();

        static bool are_equal(
            const Chorus& a,
            const Chorus& b);
    }; // Chorus

    struct Compressor
    {
        static constexpr auto min_on_off = false;
        static constexpr auto max_on_off = true;
        static constexpr auto default_on_off = true;


        bool on_off_;


        void set_defaults();

        void normalize();


        static bool are_equal(
            const Compressor& a,
            const Compressor& b);
    }; // Compressor

    struct Dedicated
    {
        static constexpr auto min_gain = 0.0F;
        static constexpr auto max_gain = 1.0F;
        static constexpr auto default_gain = 1.0F;


        float gain_;


        void set_defaults();

        void normalize();


        static bool are_equal(
            const Dedicated& a,
            const Dedicated& b);
    }; // Dedicated

    struct Distortion
    {
        static constexpr auto min_edge = 0.0F;
        static constexpr auto max_edge = 1.0F;
        static constexpr auto default_edge = 0.2F;

        static constexpr auto min_gain = 0.01F;
        static constexpr auto max_gain = 1.0F;
        static constexpr auto default_gain = 0.05F;

        static constexpr auto min_low_pass_cutoff = 80.0F;
        static constexpr auto max_low_pass_cutoff = 24000.0F;
        static constexpr auto default_low_pass_cutoff = 8000.0F;

        static constexpr auto min_eq_center = 80.0F;
        static constexpr auto max_eq_center = 24000.0F;
        static constexpr auto default_eq_center = 3600.0F;

        static constexpr auto min_eq_bandwidth = 80.0F;
        static constexpr auto max_eq_bandwidth = 24000.0F;
        static constexpr auto default_eq_bandwidth = 3600.0F;


        float edge_;
        float gain_;
        float low_pass_cutoff_;
        float eq_center_;
        float eq_bandwidth_;


        void set_defaults();

        void normalize();


        static bool are_equal(
            const Distortion& a,
            const Distortion& b);
    }; // Distortion

    struct Echo
    {
        static constexpr auto min_delay = 0.0F;
        static constexpr auto max_delay = 0.207F;
        static constexpr auto default_delay = 0.1F;

        static constexpr auto min_lr_delay = 0.0F;
        static constexpr auto max_lr_delay = 0.404F;
        static constexpr auto default_lr_delay = 0.1F;

        static constexpr auto min_damping = 0.0F;
        static constexpr auto max_damping = 0.99F;
        static constexpr auto default_damping = 0.5F;

        static constexpr auto min_feedback = 0.0F;
        static constexpr auto max_feedback = 1.0F;
        static constexpr auto default_feedback = 0.5F;

        static constexpr auto min_spread = -1.0F;
        static constexpr auto max_spread = 1.0F;
        static constexpr auto default_spread = -1.0F;


        float delay_;
        float lr_delay_;

        float damping_;
        float feedback_;

        float spread_;


        void set_defaults();

        void normalize();


        static bool are_equal(
            const Echo& a,
            const Echo& b);
    }; // Echo

    struct Equalizer
    {
        static constexpr auto min_low_gain = 0.126F;
        static constexpr auto max_low_gain = 7.943F;
        static constexpr auto default_low_gain = 1.0F;

        static constexpr auto min_low_cutoff = 50.0F;
        static constexpr auto max_low_cutoff = 800.0F;
        static constexpr auto default_low_cutoff = 200.0F;

        static constexpr auto min_mid1_gain = 0.126F;
        static constexpr auto max_mid1_gain = 7.943F;
        static constexpr auto default_mid1_gain = 1.0F;

        static constexpr auto min_mid1_center = 200.0F;
        static constexpr auto max_mid1_center = 3000.0F;
        static constexpr auto default_mid1_center = 500.0F;

        static constexpr auto min_mid1_width = 0.01F;
        static constexpr auto max_mid1_width = 1.0F;
        static constexpr auto default_mid1_width = 1.0F;

        static constexpr auto min_mid2_gain = 0.126F;
        static constexpr auto max_mid2_gain = 7.943F;
        static constexpr auto default_mid2_gain = 1.0F;

        static constexpr auto min_mid2_center = 1000.0F;
        static constexpr auto max_mid2_center = 8000.0F;
        static constexpr auto default_mid2_center = 3000.0F;

        static constexpr auto min_mid2_width = 0.01F;
        static constexpr auto max_mid2_width = 1.0F;
        static constexpr auto default_mid2_width = 1.0F;

        static constexpr auto min_high_gain = 0.126F;
        static constexpr auto max_high_gain = 7.943F;
        static constexpr auto default_high_gain = 1.0F;

        static constexpr auto min_high_cutoff = 4000.0F;
        static constexpr auto max_high_cutoff = 16000.0F;
        static constexpr auto default_high_cutoff = 6000.0F;


        float low_cutoff_;
        float low_gain_;
        float mid1_center_;
        float mid1_gain_;
        float mid1_width_;
        float mid2_center_;
        float mid2_gain_;
        float mid2_width_;
        float high_cutoff_;
        float high_gain_;


        void set_defaults();

        void normalize();


        static bool are_equal(
            const Equalizer& a,
            const Equalizer& b);
    }; // Equalizer

    struct Flanger
    {
        static constexpr auto waveform_sinusoid = 0;
        static constexpr auto waveform_triangle = 1;

        static constexpr auto min_waveform = waveform_sinusoid;
        static constexpr auto max_waveform = waveform_triangle;
        static constexpr auto default_waveform = waveform_triangle;

        static constexpr auto min_phase = -180;
        static constexpr auto max_phase = 180;
        static constexpr auto default_phase = 0;

        static constexpr auto min_rate = 0.0F;
        static constexpr auto max_rate = 10.0F;
        static constexpr auto default_rate = 0.27F;

        static constexpr auto min_depth = 0.0F;
        static constexpr auto max_depth = 1.0F;
        static constexpr auto default_depth = 1.0F;

        static constexpr auto min_feedback = -1.0F;
        static constexpr auto max_feedback = 1.0F;
        static constexpr auto default_feedback = -0.5F;

        static constexpr auto min_delay = 0.0F;
        static constexpr auto max_delay = 0.004F;
        static constexpr auto default_delay = 0.002F;


        int waveform_;
        int phase_;
        float rate_;
        float depth_;
        float feedback_;
        float delay_;


        void set_defaults();

        void normalize();


        static bool are_equal(
            const Flanger& a,
            const Flanger& b);
    }; // Flanger

    struct Reverb
    {
        static constexpr auto min_density = 0.0F;
        static constexpr auto max_density = 1.0F;
        static constexpr auto default_density = 1.0F;

        static constexpr auto min_diffusion = 0.0F;
        static constexpr auto max_diffusion = 1.0F;
        static constexpr auto default_diffusion = 1.0F;

        static constexpr auto min_gain = 0.0F;
        static constexpr auto max_gain = 1.0F;
        static constexpr auto default_gain = 0.32F;

        static constexpr auto min_gain_hf = 0.0F;
        static constexpr auto max_gain_hf = 1.0F;
        static constexpr auto default_gain_hf = 0.89F;

        static constexpr auto min_gain_lf = 0.0F;
        static constexpr auto max_gain_lf = 1.0F;
        static constexpr auto default_gain_lf = 1.0F;

        static constexpr auto min_decay_time = 0.1F;
        static constexpr auto max_decay_time = 20.0F;
        static constexpr auto default_decay_time = 1.49F;

        static constexpr auto min_decay_hf_ratio = 0.1F;
        static constexpr auto max_decay_hf_ratio = 2.0F;
        static constexpr auto default_decay_hf_ratio = 0.83F;

        static constexpr auto min_decay_lf_ratio = 0.1F;
        static constexpr auto max_decay_lf_ratio = 2.0F;
        static constexpr auto default_decay_lf_ratio = 1.0F;

        static constexpr auto min_reflections_gain = 0.0F;
        static constexpr auto max_reflections_gain = 3.16F;
        static constexpr auto default_reflections_gain = 0.05F;

        static constexpr auto min_reflections_delay = 0.0F;
        static constexpr auto max_reflections_delay = 0.3F;
        static constexpr auto default_reflections_delay = 0.007F;

        static constexpr auto min_reflections_pan_xyz = -1.0F;
        static constexpr auto max_reflections_pan_xyz = 1.0F;
        static constexpr auto default_reflections_pan_xyz = 0.0F;

        static constexpr auto min_late_reverb_gain = 0.0F;
        static constexpr auto max_late_reverb_gain = 10.0F;
        static constexpr auto default_late_reverb_gain = 1.26F;

        static constexpr auto min_late_reverb_delay = 0.0F;
        static constexpr auto max_late_reverb_delay = 0.1F;
        static constexpr auto default_late_reverb_delay = 0.011F;

        static constexpr auto min_late_reverb_pan_xyz = -1.0F;
        static constexpr auto max_late_reverb_pan_xyz = 1.0F;
        static constexpr auto default_late_reverb_pan_xyz = 0.0F;

        static constexpr auto min_echo_time = 0.075F;
        static constexpr auto max_echo_time = 0.25F;
        static constexpr auto default_echo_time = 0.25F;

        static constexpr auto min_echo_depth = 0.0F;
        static constexpr auto max_echo_depth = 1.0F;
        static constexpr auto default_echo_depth = 0.0F;

        static constexpr auto min_modulation_time = 0.04F;
        static constexpr auto max_modulation_time = 4.0F;
        static constexpr auto default_modulation_time = 0.25F;

        static constexpr auto min_modulation_depth = 0.0F;
        static constexpr auto max_modulation_depth = 1.0F;
        static constexpr auto default_modulation_depth = 0.0F;

        static constexpr auto min_air_absorption_gain_hf = 0.892F;
        static constexpr auto max_air_absorption_gain_hf = 1.0F;
        static constexpr auto default_air_absorption_gain_hf = 0.994F;

        static constexpr auto min_hf_reference = 1000.0F;
        static constexpr auto max_hf_reference = 20000.0F;
        static constexpr auto default_hf_reference = 5000.0F;

        static constexpr auto min_lf_reference = 20.0F;
        static constexpr auto max_lf_reference = 1000.0F;
        static constexpr auto default_lf_reference = 250.0F;

        static constexpr auto min_room_rolloff_factor = 0.0F;
        static constexpr auto max_room_rolloff_factor = 10.0F;
        static constexpr auto default_room_rolloff_factor = 0.0F;

        static constexpr auto min_decay_hf_limit = false;
        static constexpr auto max_decay_hf_limit = true;
        static constexpr auto default_decay_hf_limit = true;


        float density_;
        float diffusion_;
        float gain_;
        float gain_hf_;
        float gain_lf_; // EAX
        float decay_time_;
        float decay_hf_ratio_;
        float decay_lf_ratio_; // EAX
        float reflections_gain_;
        float reflections_delay_;
        Pan reflections_pan_; // EAX
        float late_reverb_gain_;
        float late_reverb_delay_;
        Pan late_reverb_pan_; // EAX
        float echo_time_; // EAX
        float echo_depth_; // EAX
        float modulation_time_; // EAX
        float modulation_depth_; // EAX
        float air_absorption_gain_hf_;
        float hf_reference_; // EAX
        float lf_reference_; // EAX
        float room_rolloff_factor_;
        bool decay_hf_limit_;


        void set_defaults();

        void normalize();


        static bool are_equal(
            const Reverb& a,
            const Reverb& b);
    }; // Reverb

    struct RingModulator
    {
        static constexpr auto min_frequency = 0.0F;
        static constexpr auto max_frequency = 8000.0F;
        static constexpr auto default_frequency = 440.0F;

        static constexpr auto min_high_pass_cutoff = 0.0F;
        static constexpr auto max_high_pass_cutoff = 24000.0F;
        static constexpr auto default_high_pass_cutoff = 800.0F;

        static constexpr auto waveform_sinusoid = 0;
        static constexpr auto waveform_sawtooth = 1;
        static constexpr auto waveform_square = 2;

        static constexpr auto min_waveform = waveform_sinusoid;
        static constexpr auto max_waveform = waveform_square;
        static constexpr auto default_waveform = waveform_sinusoid;


        float frequency_;
        float high_pass_cutoff_;
        int waveform_;


        void set_defaults();

        void normalize();


        static bool are_equal(
            const RingModulator& a,
            const RingModulator& b);
    }; // RingModulator


    Chorus chorus_;
    Compressor compressor_;
    Dedicated dedicated_;
    Distortion distortion_;
    Echo echo_;
    Equalizer equalizer_;
    Flanger flanger_;
    Reverb reverb_;
    RingModulator ring_modulator_;
}; // EffectProps

struct Effect
{
    EffectType type_;
    EffectProps props_;


    void set_defaults();

    void set_type_and_defaults(
        const EffectType effect_type);

    void normalize();

    static bool are_equal(
        const Effect& a,
        const Effect& b);
}; // Effect

struct SendProps
{
    static constexpr auto lp_frequency_reference = 5'000.0F;
    static constexpr auto hp_frequency_reference = 250.0F;

    static constexpr auto min_gain = 0.0F;
    static constexpr auto max_gain = 1.0F;
    static constexpr auto default_gain = 1.0F;

    static constexpr auto min_gain_hf = 0.0F;
    static constexpr auto max_gain_hf = 1.0F;
    static constexpr auto default_gain_hf = 1.0F;

    static constexpr auto min_gain_lf = 0.0F;
    static constexpr auto max_gain_lf = 1.0F;
    static constexpr auto default_gain_lf = 1.0F;


    float gain_;
    float gain_hf_;
    float gain_lf_;


    void set_defaults();

    void normalize();


    static bool are_equal(
        const SendProps& a,
        const SendProps& b);
}; // SendProps

struct ReverbPresets
{
    struct Default
    {
        static const EffectProps::Reverb generic;
        static const EffectProps::Reverb padded_cell;
        static const EffectProps::Reverb room;
        static const EffectProps::Reverb bathroom;
        static const EffectProps::Reverb living_room;
        static const EffectProps::Reverb stone_room;
        static const EffectProps::Reverb auditorium;
        static const EffectProps::Reverb concert_hall;
        static const EffectProps::Reverb cave;
        static const EffectProps::Reverb arena;
        static const EffectProps::Reverb hangar;
        static const EffectProps::Reverb carpeted_hallway;
        static const EffectProps::Reverb hallway;
        static const EffectProps::Reverb stone_corridor;
        static const EffectProps::Reverb alley;
        static const EffectProps::Reverb forest;
        static const EffectProps::Reverb city;
        static const EffectProps::Reverb mountains;
        static const EffectProps::Reverb quarry;
        static const EffectProps::Reverb plain;
        static const EffectProps::Reverb parking_lot;
        static const EffectProps::Reverb sewer_pipe;
        static const EffectProps::Reverb underwater;
        static const EffectProps::Reverb drugged;
        static const EffectProps::Reverb dizzy;
        static const EffectProps::Reverb psychotic;
    }; // Default

    struct Castle
    {
        static const EffectProps::Reverb small_room;
        static const EffectProps::Reverb short_passage;
        static const EffectProps::Reverb medium_room;
        static const EffectProps::Reverb large_room;
        static const EffectProps::Reverb long_passage;
        static const EffectProps::Reverb hall;
        static const EffectProps::Reverb cupboard;
        static const EffectProps::Reverb courtyard;
        static const EffectProps::Reverb alcove;
    }; // Castle

    struct Factory
    {
        static const EffectProps::Reverb small_room;
        static const EffectProps::Reverb short_passage;
        static const EffectProps::Reverb medium_room;
        static const EffectProps::Reverb large_room;
        static const EffectProps::Reverb long_passage;
        static const EffectProps::Reverb hall;
        static const EffectProps::Reverb cupboard;
        static const EffectProps::Reverb courtyard;
        static const EffectProps::Reverb alcove;
    }; // Factory

    struct IcePalace
    {
        static const EffectProps::Reverb small_room;
        static const EffectProps::Reverb short_passage;
        static const EffectProps::Reverb medium_room;
        static const EffectProps::Reverb large_room;
        static const EffectProps::Reverb long_passage;
        static const EffectProps::Reverb hall;
        static const EffectProps::Reverb cupboard;
        static const EffectProps::Reverb courtyard;
        static const EffectProps::Reverb alcove;
    }; // IcePalace

    struct SpaceStation
    {
        static const EffectProps::Reverb small_room;
        static const EffectProps::Reverb short_passage;
        static const EffectProps::Reverb medium_room;
        static const EffectProps::Reverb large_room;
        static const EffectProps::Reverb long_passage;
        static const EffectProps::Reverb hall;
        static const EffectProps::Reverb cupboard;
        static const EffectProps::Reverb alcove;
    }; // SpaceStation

    struct WoodenGaleon
    {
        static const EffectProps::Reverb small_room;
        static const EffectProps::Reverb short_passage;
        static const EffectProps::Reverb medium_room;
        static const EffectProps::Reverb large_room;
        static const EffectProps::Reverb long_passage;
        static const EffectProps::Reverb hall;
        static const EffectProps::Reverb cupboard;
        static const EffectProps::Reverb courtyard;
        static const EffectProps::Reverb alcove;
    }; // WoodenGaleon

    struct Sports
    {
        static const EffectProps::Reverb empty_stadium;
        static const EffectProps::Reverb squash_court;
        static const EffectProps::Reverb small_swimming_pool;
        static const EffectProps::Reverb large_swimming_pool;
        static const EffectProps::Reverb gymnasium;
        static const EffectProps::Reverb full_stadium;
        static const EffectProps::Reverb stadium_tannoy;
    }; // Sports

    struct Prefab
    {
        static const EffectProps::Reverb workshop;
        static const EffectProps::Reverb school_room;
        static const EffectProps::Reverb practise_room;
        static const EffectProps::Reverb outhouse;
        static const EffectProps::Reverb caravan;
    }; // Prefab

    struct Dome
    {
        static const EffectProps::Reverb tomb;
        static const EffectProps::Reverb saint_pauls;
    }; // Dome

    struct Pipe
    {
        static const EffectProps::Reverb small;
        static const EffectProps::Reverb long_thin;
        static const EffectProps::Reverb large;
        static const EffectProps::Reverb resonant;
    }; // Pipe

    struct Outdoors
    {
        static const EffectProps::Reverb backyard;
        static const EffectProps::Reverb rolling_plains;
        static const EffectProps::Reverb deep_canyon;
        static const EffectProps::Reverb creek;
        static const EffectProps::Reverb valley;
    }; // Outdoors

    struct Mood
    {
        static const EffectProps::Reverb heaven;
        static const EffectProps::Reverb hell;
        static const EffectProps::Reverb memory;
    }; // Mood

    struct Driving
    {
        static const EffectProps::Reverb commentator;
        static const EffectProps::Reverb pit_garage;
        static const EffectProps::Reverb incar_racer;
        static const EffectProps::Reverb incar_sports;
        static const EffectProps::Reverb incar_luxury;
        static const EffectProps::Reverb full_grand_stand;
        static const EffectProps::Reverb empty_grand_stand;
        static const EffectProps::Reverb tunnel;
    }; // Driving

    struct City
    {
        static const EffectProps::Reverb streets;
        static const EffectProps::Reverb subway;
        static const EffectProps::Reverb museum;
        static const EffectProps::Reverb library;
        static const EffectProps::Reverb underpass;
        static const EffectProps::Reverb abandoned;
    }; // City

    struct Misc
    {
        static const EffectProps::Reverb dusty_room;
        static const EffectProps::Reverb chapel;
        static const EffectProps::Reverb small_water_room;
    }; // Misc
}; // ReverbPresets


class Api
{
public:
    Api();

    Api(
        const Api& that) = delete;

    Api& operator=(
        const Api& that) = delete;

    ~Api();


    // Initializes the instance.
    //
    // Returns true on success or false otherwise.
    bool initialize(
        const ChannelFormat channel_format,
        const int sampling_rate,
        const int effect_count);

    // Gets instance's initialization flag.
    //
    // Returns true if the instance is initialized or false otherwise.
    bool is_initialized() const;

    // Gets a sampling rate.
    //
    // Returns a sampling rate or zero on error.
    int get_sampling_rate() const;

    // Gets a channel format.
    //
    // Returns a channel format or "none" on error.
    ChannelFormat get_channel_format() const;

    // Gets a channel count.
    //
    // Returns a channel count or zero on error.
    int get_channel_count() const;

    // Gets an effect count.
    //
    // Returns an effect count or zero on error.
    int get_effect_count() const;

    // Gets the active effect's properties.
    //
    // Returns true on success or false otherwise.
    bool get_effect(
        const int effect_index,
        Effect& effect) const;

    // Gets the deferred effect's properties.
    //
    // Returns true on success or false otherwise.
    bool get_deferred_effect(
        const int effect_index,
        Effect& effect) const;

    // Sets the deferred effect's properties.
    //
    // Returns true on success or false otherwise.
    bool set_effect_type(
        const int effect_index,
        const EffectType effect_type);

    // Sets the deferred effect's properties.
    //
    // Returns true on success or false otherwise.
    bool set_effect_props(
        const int effect_index,
        const EffectProps& effect_props);

    // Sets the deferred effect's type and the properties.
    //
    // Returns true on success or false otherwise.
    bool set_effect(
        const int effect_index,
        const Effect& effect);

    // Gets the active send's properties.
    //
    // Returns true on success or false otherwise.
    bool get_send_props(
        const int effect_index,
        SendProps& send_props) const;

    // Gets the deferred send's properties.
    //
    // Returns true on success or false otherwise.
    bool get_deferred_send_props(
        const int effect_index,
        SendProps& send_props) const;

    // Sets the deferred send's properties.
    //
    // Returns true on success or false otherwise.
    bool set_send_props(
        const int effect_index,
        const SendProps& send_props);

    // Applies all deferred changes.
    //
    // Returns true on success or false otherwise.
    bool apply_changes();

    // Mixes samples from the source buffer into the target one.
    // !!!WARNING!!! Mixed samples are NOT CLIPPED.
    //
    // Returns true on success or false otherwise.
    bool mix(
        const int sample_count,
        const float* src_samples,
        float* dst_samples);

    // Uninitializes the instance.
    void uninitialize();

    // Gets a last error message.
    const char* get_error_message() const;


    // Gets the minimum allowed channel count.
    static int get_min_channels();

    // Gets the maximum allowed channel count.
    static int get_max_channels();

    // Gets the minimum allowed sampling rate.
    static int get_min_sampling_rate();

    // Gets the maximum allowed sampling rate.
    static int get_max_sampling_rate();

    // Gets the minimum allowed effect count.
    static int get_min_effects();

    // Gets the maximum allowed effect count.
    static int get_max_effects();

    // Converts the channel count into the channel format.
    //
    // Returns a channel format or "none" on error.
    static ChannelFormat channel_count_to_channel_format(
        const int channel_count);

    // Converts the channel format into the channel count.
    //
    // Returns a channel count or zero on error.
    static int channel_format_to_channel_count(
        const ChannelFormat channel_format);


private:
    class Impl;
    using ApiImplUPtr = std::unique_ptr<Impl>;


    ApiImplUPtr pimpl_;
    mutable const char* error_message_;
}; // Api


} // oalsfxpp


#endif // OALSFXPP_INCLUDED
