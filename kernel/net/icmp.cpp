#include "icmp.hpp"
#include "ipv4.hpp"
#include "drivers/vga.hpp"
#include <string.h>

namespace MesaOS::Net {

static uint16_t calculate_checksum(void* v_addr, uint32_t count) {
    uint16_t* addr = (uint16_t*)v_addr;
    uint32_t sum = 0;
    while (count > 1) {
        sum += *addr++;
        count -= 2;
    }
    if (count > 0) sum += *(uint8_t*)addr;
    while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
    return ~((uint16_t)sum);
}

void ICMP::send_echo_request(uint32_t dest_ip, uint16_t id, uint16_t sequence) {
    uint32_t packet_size = sizeof(ICMPHeader) + 32; // Header + 32 bytes of data
    uint8_t packet[packet_size];
    memset(packet, 0, packet_size);

    ICMPHeader* header = (ICMPHeader*)packet;
    header->type = 8; // Echo Request
    header->code = 0;
    header->id = (id << 8) | (id >> 8); // Swap for network order? Actually checksum is sensitive. Let's keep simpler.
    // Standard ping sends BE.
    header->id = ((id >> 8) & 0xFF) | ((id << 8) & 0xFF00);
    header->sequence = ((sequence >> 8) & 0xFF) | ((sequence << 8) & 0xFF00);
    header->checksum = 0;

    // Fill payload with some alphabet
    char* payload = (char*)(packet + sizeof(ICMPHeader));
    for(int i=0; i<32; i++) payload[i] = 'a' + (i % 26);

    header->checksum = calculate_checksum(packet, packet_size);

    // Protocol 1 is ICMP
    IPv4::send_packet(dest_ip, 1, packet, packet_size);
}

void ICMP::handle_packet(uint8_t* data, uint32_t size, uint32_t src_ip) {
    if (size < sizeof(ICMPHeader)) return;
    ICMPHeader* msg = (ICMPHeader*)data;

    MesaOS::Drivers::VGADriver vga;

    if (msg->type == 0) { // Echo Reply
        char b[16];
        vga.write_string("Reply from ");
        
        // IP printing util
        vga.write_string(itoa(src_ip & 0xFF, b, 10)); vga.write_string(".");
        vga.write_string(itoa((src_ip >> 8) & 0xFF, b, 10)); vga.write_string(".");
        vga.write_string(itoa((src_ip >> 16) & 0xFF, b, 10)); vga.write_string(".");
        vga.write_string(itoa((src_ip >> 24) & 0xFF, b, 10));

        vga.write_string(": bytes=");
        vga.write_string(itoa(size - sizeof(ICMPHeader), b, 10));
        vga.write_string(" seq=");
        uint16_t seq = ((msg->sequence >> 8) & 0xFF) | ((msg->sequence << 8) & 0xFF00);
        vga.write_string(itoa(seq, b, 10));
        vga.write_string(" ttl=??\n"); // TTL is in IP header, lost here but fine
    }
}

} // namespace MesaOS::Net
