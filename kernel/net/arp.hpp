#ifndef ARP_HPP
#define ARP_HPP

#include <stdint.h>

namespace MesaOS::Net {

struct ARPMessage {
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t hardware_addr_len;
    uint8_t protocol_addr_len;
    uint16_t opcode;
    uint8_t src_mac[6];
    uint32_t src_ip;
    uint8_t dest_mac[6];
    uint32_t dest_ip;
} __attribute__((packed));

class ARP {
public:
    static void handle_packet(uint8_t* data, uint32_t size);
    static void send_request(uint32_t target_ip);
    static void send_reply(uint8_t* dest_mac, uint32_t dest_ip);
    static uint8_t* resolve(uint32_t target_ip);

private:
    static uint32_t cache_ip;
    static uint8_t cache_mac[6];
};

} // namespace MesaOS::Net

#endif
