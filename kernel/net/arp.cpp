#include "arp.hpp"
#include "ethernet.hpp"
#include "ipv4.hpp"
#include "drivers/rtl8139.hpp"
#include <string.h>

namespace MesaOS::Net {

uint32_t ARP::cache_ip = 0;
uint8_t ARP::cache_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static uint8_t broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

static inline uint16_t swap_uint16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

void ARP::handle_packet(uint8_t* data, uint32_t size) {
    if (size < sizeof(ARPMessage)) return;

    ARPMessage* msg = (ARPMessage*)data;
    uint16_t opcode = swap_uint16(msg->opcode);
    
    // Update Cache if it's a reply or request for us
    if (msg->dest_ip == IPv4::get_ip()) {
        cache_ip = msg->src_ip;
        memcpy(cache_mac, msg->src_mac, 6);
    }

    if (opcode == 0x0100) { // Request (swapped 1)
        if (msg->dest_ip == IPv4::get_ip()) {
            send_reply(msg->src_mac, msg->src_ip);
        }
    }
}

void ARP::send_reply(uint8_t* dest_mac, uint32_t dest_ip) {
    ARPMessage reply;
    reply.hardware_type = swap_uint16(1); // Ethernet
    reply.protocol_type = swap_uint16(0x0800); // IPv4
    reply.hardware_addr_len = 6;
    reply.protocol_addr_len = 4;
    reply.opcode = swap_uint16(2); // Reply

    memcpy(reply.src_mac, MesaOS::Drivers::RTL8139::get_mac_address(), 6);
    reply.src_ip = IPv4::get_ip();
    memcpy(reply.dest_mac, dest_mac, 6);
    reply.dest_ip = dest_ip;

    Ethernet::send_packet(dest_mac, 0x0806, (uint8_t*)&reply, sizeof(ARPMessage));
}

uint8_t* ARP::resolve(uint32_t target_ip) {
    if (target_ip == cache_ip) {
        return cache_mac;
    }
    
    // Not in cache, send request and return broadcast for now
    send_request(target_ip);
    return broadcast_mac;
}

void ARP::send_request(uint32_t target_ip) {
    ARPMessage req;
    req.hardware_type = swap_uint16(1);
    req.protocol_type = swap_uint16(0x0800);
    req.hardware_addr_len = 6;
    req.protocol_addr_len = 4;
    req.opcode = swap_uint16(1); // Request

    memcpy(req.src_mac, Ethernet::get_mac_address(), 6);
    req.src_ip = IPv4::get_ip();
    memset(req.dest_mac, 0, 6);
    req.dest_ip = target_ip;

    // Minimum Ethernet payload is 46 bytes (to make 60 bytes frame without FCS)
    uint8_t packet[46]; 
    memset(packet, 0, 46); // Padding
    memcpy(packet, &req, sizeof(ARPMessage));

    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    Ethernet::send_packet(broadcast_mac, 0x0806, packet, 46);
}

} // namespace MesaOS::Net
