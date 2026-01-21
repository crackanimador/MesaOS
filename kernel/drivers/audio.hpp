#ifndef AUDIO_HPP
#define AUDIO_HPP

#include <stdint.h>
#include "pci.hpp"

namespace MesaOS::Drivers {

struct AudioFormat {
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
};

class AudioDriver {
public:
    static void initialize();
    static bool is_present();
    static bool set_format(const AudioFormat* format);
    static bool play_buffer(const uint8_t* buffer, uint32_t size);
    static bool record_buffer(uint8_t* buffer, uint32_t size);
    static bool set_volume(uint8_t volume);
    static uint8_t get_volume();
    static bool is_playing();
    static bool is_recording();
    static void stop_playback();
    static void stop_recording();

private:
    static bool initialized;
    static AudioFormat current_format;
    static uint8_t volume;
    static bool playing;
    static bool recording;
    static PCIDevice audio_device;

    // PC Speaker functions
    static void play_melody_pc_speaker(const uint8_t* buffer, uint32_t size);
    static void play_tone_pc_speaker(uint16_t frequency, uint32_t duration_ms);
    static void enable_pc_speaker();
    static void disable_pc_speaker();
    static void set_pit_frequency(uint16_t frequency);
};

} // namespace MesaOS::Drivers

#endif
