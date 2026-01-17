#include "ethernet.hpp"
#include "arp.hpp"
#include "ipv4.hpp"
#include "drivers/rtl8139.hpp"
#include "drivers/pcnet.hpp"
#include "drivers/vga.hpp"
#include "memory/kheap.hpp"
#include <string.h>

namespace MesaOS::Net {

// Swap byte order for 16-bit values
static inline uint16_t swap_uint16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

void Ethernet::handle_packet(uint8_t* data, uint32_t size) {
    if (size < sizeof(EthernetHeader)) return;

    EthernetHeader* header = (EthernetHeader*)data;
    uint16_t type = header->type; // Don't swap yet, check raw values first
    uint8_t* payload = data + sizeof(EthernetHeader);
    uint32_t payload_size = size - sizeof(EthernetHeader);

    if (type == 0x0608 || type == 0x0806 || type == swap_uint16(0x0806)) { // ARP (0x0806)
        ARP::handle_packet(payload, payload_size);
    } else if (type == 0x0008 || type == 0x0800 || type == swap_uint16(0x0800)) { // IPv4 (0x0800)
        IPv4::handle_packet(payload, payload_size);
    } else {
        // Unknown type debug
        // MesaOS::Drivers::VGADriver vga;
        // char b[16]; vga.write_string("[UNK: "); vga.write_string(itoa(type, b, 16)); vga.write_string("] ");
    }
}

void Ethernet::send_packet(uint8_t* dest_mac, uint16_t type, uint8_t* data, uint32_t size) {
    uint8_t* buffer = (uint8_t*)kmalloc(size + sizeof(EthernetHeader));
    EthernetHeader* header = (EthernetHeader*)buffer;

    memcpy(header->dest_mac, dest_mac, 6);
    memcpy(header->src_mac, get_mac_address(), 6);
    header->type = swap_uint16(type); // FIX: Send as Big Endian (Network Order)

    // Copy payload
    memcpy(buffer + sizeof(EthernetHeader), data, size);

    MesaOS::Drivers::RTL8139::send_packet(buffer, size + sizeof(EthernetHeader));
    MesaOS::Drivers::PCNet::send_packet(buffer, size + sizeof(EthernetHeader));
    
    // In a real OS we'd use a better memory management for packets
    // kfree(buffer); 
}

uint8_t* Ethernet::get_mac_address() {
    uint8_t* mac = MesaOS::Drivers::RTL8139::get_mac_address();
    for(int i = 0; i < 6; i++) if(mac[i] != 0) return mac;
    
    mac = MesaOS::Drivers::PCNet::get_mac_address();
    return mac; // Return PCNet if RTL is all zeros
}

} // namespace MesaOS::Net
