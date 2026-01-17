#ifndef IPV4_HPP
#define IPV4_HPP

#include <stdint.h>

namespace MesaOS::Net {

struct IPv4Header {
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t length;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} __attribute__((packed));

class IPv4 {
public:
    static void handle_packet(uint8_t* data, uint32_t size);
    static void send_packet(uint32_t dest_ip, uint8_t protocol, uint8_t* data, uint32_t size);
    
    static void set_ip(uint32_t ip);
    static uint32_t get_ip();
    static void set_gateway(uint32_t ip);
    static uint32_t get_gateway();
    static void set_netmask(uint32_t ip);
    static uint32_t get_netmask();

private:
    static uint32_t my_ip;
    static uint32_t gateway_ip;
    static uint32_t subnet_mask;
};

} // namespace MesaOS::Net

#endif
