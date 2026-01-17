#ifndef ETHERNET_HPP
#define ETHERNET_HPP

#include <stdint.h>

namespace MesaOS::Net {

struct EthernetHeader {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t type;
} __attribute__((packed));

class Ethernet {
public:
    static void handle_packet(uint8_t* data, uint32_t size);
    static void send_packet(uint8_t* dest_mac, uint16_t type, uint8_t* data, uint32_t size);
    static uint8_t* get_mac_address();
};

} // namespace MesaOS::Net

#endif
