#ifndef WIFI_HPP
#define WIFI_HPP

#include <stdint.h>

namespace MesaOS::Drivers {

class WiFiDriver {
public:
    static void initialize();
    static bool is_present();
    static bool scan_networks();
    static bool connect(const char* ssid, const char* password);
    static bool disconnect();
    static bool send_packet(const uint8_t* data, uint32_t size);
    static uint32_t receive_packet(uint8_t* buffer, uint32_t size);
    static const char* get_connected_ssid();
    static int get_signal_strength();

private:
    static bool initialized;
    static bool connected;
    static char current_ssid[33];
    static int signal_strength;
};

} // namespace MesaOS::Drivers

#endif
