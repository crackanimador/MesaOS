#ifndef UDP_HPP
#define UDP_HPP

#include <stdint.h>

namespace MesaOS::Net {

struct UDPHeader {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed));

class UDP {
public:
    static void handle_packet(uint32_t src_ip, uint8_t* data, uint32_t size);
    static void send_packet(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, uint8_t* data, uint32_t size);
    static void handle_packet_ipv6(const uint8_t* src_ip, uint8_t* data, uint32_t size); // Placeholder
};

} // namespace MesaOS::Net

#endif
