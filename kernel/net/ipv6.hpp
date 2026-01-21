#ifndef MESAOS_NET_IPV6_HPP
#define MESAOS_NET_IPV6_HPP

#include <stdint.h>

namespace MesaOS::Net {

#pragma pack(push, 1)
struct IPv6Header {
    uint32_t version_traffic_flow;  // Version (4 bits), Traffic class (8 bits), Flow label (20 bits)
    uint16_t payload_length;
    uint8_t next_header;
    uint8_t hop_limit;
    uint8_t src_addr[16];
    uint8_t dest_addr[16];
};
#pragma pack(pop)

// IPv6 constants
#define IPV6_VERSION 6
#define IPV6_ADDR_SIZE 16

class IPv6 {
public:
    static void initialize();
    static void handle_packet(uint8_t* data, uint32_t size);
    static void send_packet(const uint8_t* dest_addr, uint8_t next_header, const uint8_t* payload, uint32_t payload_size);
    static const uint8_t* get_link_local_addr();
    static void set_global_addr(const uint8_t* addr);

private:
    static uint8_t link_local_addr[16];
    static uint8_t global_addr[16];
    static bool has_global_addr;

    // Neighbor cache (simplified)
    static uint8_t neighbor_cache[8][6]; // MAC addresses for IPv6 addresses
    static uint8_t neighbor_addrs[8][16]; // IPv6 addresses
    static uint8_t neighbor_count;

    static uint16_t calculate_checksum(const IPv6Header* header, const uint8_t* payload, uint32_t payload_size);
    static void handle_icmpv6_packet(const uint8_t* src_addr, uint8_t* data, uint32_t size);
    static void send_neighbor_advertisement(const uint8_t* dest_addr, const uint8_t* target_addr);
    static void update_neighbor_cache(const uint8_t* ip_addr, const uint8_t* mac_addr);
};

} // namespace MesaOS::Net

#endif // MESAOS_NET_IPV6_HPP
