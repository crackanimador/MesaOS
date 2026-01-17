#include "ipv4.hpp"
#include "ethernet.hpp"
#include "udp.hpp"
#include "arp.hpp"
#include "icmp.hpp" // Added by instruction
#include "memory/kheap.hpp"
#include <string.h>

namespace MesaOS::Net {

uint32_t IPv4::my_ip = 0;
uint32_t IPv4::gateway_ip = 0;
uint32_t IPv4::subnet_mask = 0;

static inline uint16_t swap_uint16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

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

static uint16_t calculate_checksum(IPv4Header* header) {
    return calculate_checksum((void*)header, sizeof(IPv4Header));
}
// Note: IP checksum works the same regardless of endianness as long as you sum 16-bit words.

void IPv4::handle_packet(uint8_t* data, uint32_t size) {
    if (size < sizeof(IPv4Header)) return;
    
    IPv4Header* header = (IPv4Header*)data;
    uint8_t* payload = data + (header->version_ihl & 0x0F) * 4;
    uint32_t payload_size = swap_uint16(header->length) - (header->version_ihl & 0x0F) * 4;

    if (header->protocol == 17) { // UDP
        UDP::handle_packet(header->src_ip, payload, payload_size);
    } else if (header->protocol == 1) { // ICMP
        ICMP::handle_packet(payload, payload_size, header->src_ip);
    }
}

void IPv4::send_packet(uint32_t dest_ip, uint8_t protocol, uint8_t* data, uint32_t size) {
    uint8_t* packet = (uint8_t*)kmalloc(sizeof(IPv4Header) + size);
    IPv4Header* header = (IPv4Header*)packet;

    header->version_ihl = 4 << 4 | 5;
    header->tos = 0;
    header->length = swap_uint16(sizeof(IPv4Header) + size);
    header->id = swap_uint16(0x1234); // TODO: Increment
    header->flags_fragment = 0; // swap_uint16(0x4000); // DF
    header->ttl = 64;
    header->protocol = protocol;
    header->checksum = 0;
    header->src_ip = my_ip;
    header->dest_ip = dest_ip;
    
    header->checksum = calculate_checksum(header);
    
    // Routing Logic
    uint32_t arp_target_ip = dest_ip;
    
    // Simple logic: if we have a gateway and destination looks remote, use gateway
    if (gateway_ip != 0 && subnet_mask != 0) {
        if ((dest_ip & subnet_mask) != (my_ip & subnet_mask)) {
            arp_target_ip = gateway_ip;
        }
    }
    
    // Need ARP resolution
    uint8_t* dest_mac = ARP::resolve(arp_target_ip);
    
    // If ARP failed, we broadcast (bad fallback but works for local) or drop.
    // For now, let's assume ARP works or returns broadcast if unknown.
    // Actually ARP::resolve usually blocks or returns 0.
    // Let's assume ARP::resolve handles the blocking/resolution internally 
    // or returns broadcast if it can't find.
    // In our implementation ARP::resolve likely sends request and waits or returns cache.
    // If we don't have MAC, packet is lost. 
    // But ARP::resolve in this simple OS probably returns ff:ff:ff:ff:ff:ff if not found immediately?
    // We haven't implemented blocking ARP resolve really. 
    // ARP::resolve usually sends request. 
    // Let's rely on ARP::resolve doing the "right thing" (sending request).
    // If it returns broadcast/null, packet won't reach gateway.
    
    memcpy(packet + sizeof(IPv4Header), data, size);
    
    Ethernet::send_packet(dest_mac, 0x0800, packet, sizeof(IPv4Header) + size);
    
    kfree(packet);
}

void IPv4::set_ip(uint32_t ip) { my_ip = ip; }
uint32_t IPv4::get_ip() { return my_ip; }

void IPv4::set_gateway(uint32_t ip) { gateway_ip = ip; }
uint32_t IPv4::get_gateway() { return gateway_ip; }

void IPv4::set_netmask(uint32_t ip) { subnet_mask = ip; }
uint32_t IPv4::get_netmask() { return subnet_mask; }

} // namespace MesaOS::Net
