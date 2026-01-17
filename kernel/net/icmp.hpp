#ifndef ICMP_HPP
#define ICMP_HPP

#include <stdint.h>

namespace MesaOS::Net {

struct ICMPHeader {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} __attribute__((packed));

class ICMP {
public:
    static void send_echo_request(uint32_t dest_ip, uint16_t id, uint16_t sequence);
    static void handle_packet(uint8_t* data, uint32_t size, uint32_t src_ip);
};

} // namespace MesaOS::Net

#endif
