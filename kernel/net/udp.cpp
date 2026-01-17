#include "udp.hpp"
#include "ipv4.hpp"
#include "dhcp.hpp"
#include "memory/kheap.hpp"
#include <string.h>

namespace MesaOS::Net {

static inline uint16_t swap_uint16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

void UDP::handle_packet(uint32_t src_ip, uint8_t* data, uint32_t size) {
    if (size < sizeof(UDPHeader)) return;

    UDPHeader* header = (UDPHeader*)data;
    uint16_t dest_port = swap_uint16(header->dest_port);
    uint8_t* payload = data + sizeof(UDPHeader);
    uint32_t payload_size = size - sizeof(UDPHeader);

    if (dest_port == 68) {
        DHCP::handle_packet(payload, payload_size);
    }

    (void)src_ip; (void)dest_port; (void)payload; (void)payload_size;
}

void UDP::send_packet(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, uint8_t* data, uint32_t size) {
    uint32_t total_size = size + sizeof(UDPHeader);
    uint8_t* buffer = (uint8_t*)kmalloc(total_size);
    UDPHeader* header = (UDPHeader*)buffer;

    header->src_port = swap_uint16(src_port);
    header->dest_port = swap_uint16(dest_port);
    header->length = swap_uint16(total_size);
    header->checksum = 0; // Optional in IPv4

    memcpy(buffer + sizeof(UDPHeader), data, size);

    IPv4::send_packet(dest_ip, 0x11, buffer, total_size);
}

} // namespace MesaOS::Net
