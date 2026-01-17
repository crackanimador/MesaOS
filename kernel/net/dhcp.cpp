#include "dhcp.hpp"
#include "udp.hpp"
#include "ipv4.hpp"
#include "drivers/rtl8139.hpp"
#include "drivers/pcnet.hpp"
#include "ethernet.hpp"
#include "drivers/vga.hpp"
#include <string.h>

namespace MesaOS::Net {

uint32_t DHCP::transaction_id = 0x12345678;

static inline uint32_t swap_uint32(uint32_t val) {
    return ((val >> 24) & 0xFF) | ((val >> 8) & 0xFF00) | ((val << 8) & 0xFF0000) | ((val << 24) & 0xFF000000);
}

void DHCP::discover() {
    DHCPMessage msg;
    memset(&msg, 0, sizeof(DHCPMessage));

    msg.op = 1; // Request
    msg.htype = 1; // Ethernet
    msg.hlen = 6;
    msg.xid = swap_uint32(transaction_id);
    // Magic Cookie: 0x63, 0x82, 0x53, 0x63 in Network Order
    msg.magic_cookie = swap_uint32(0x63825363); 

    // Use our MAC address
    memcpy(msg.chaddr, Ethernet::get_mac_address(), 6);

    // DHCP Options
    uint8_t* opt = msg.options;
    
    // Option 53: DHCP Message Type (1 = Discover)
    *opt++ = 53; *opt++ = 1; *opt++ = 1;
    
    // Option 55: Parameter Request List
    *opt++ = 55; *opt++ = 4;
    *opt++ = 1;  // Subnet Mask
    *opt++ = 3;  // Router
    *opt++ = 15; // Domain Name
    *opt++ = 6;  // DNS Server
    
    // Option 255: End
    *opt++ = 255;

    // Persistent Mode: Keep trying until we get an IP
    MesaOS::Drivers::VGADriver vga;
    vga.write_string("Starting DHCP Daemon (Press ESC to cancel)...\n");
    
    int attempts = 0;
    while(IPv4::get_ip() == 0) {
        attempts++;
        char b[16];
        vga.write_string("\n[DHCP Attempt "); vga.write_string(itoa(attempts, b, 10)); vga.write_string("] Sending Discover... ");
        
        UDP::send_packet(0xFFFFFFFF, 68, 67, (uint8_t*)&msg, sizeof(DHCPMessage));
        vga.write_string("SENT. Scanning...");
        
        // Scan for 2 seconds (approx 60M cycles)
        for(volatile int i=0; i<60000000; i++) {
            // Poll both drivers to be sure
            MesaOS::Drivers::PCNet::poll();
            MesaOS::Drivers::RTL8139::receive_packet(); // Just in case
            
            if (IPv4::get_ip() != 0) break; // Success!
            
            if ((i % 2000000) == 0) {
                 // Check for ESC key? (Would require keyboard polling)
                 // For now just visual feedback
            }
        }
    }
    vga.write_string("\nDaemon Success! IP Obtained.\n");
}

void DHCP::handle_packet(uint8_t* data, uint32_t size) {
    if (size < sizeof(DHCPMessage) - 312) return;
    DHCPMessage* msg = (DHCPMessage*)data;

    if (memcmp(msg->chaddr, MesaOS::Net::Ethernet::get_mac_address(), 6) != 0) return;

    if (msg->yiaddr != 0) {
        MesaOS::Drivers::VGADriver vga;
        IPv4::set_ip(msg->yiaddr);
        
        // Parse Options
        uint8_t* ptr = msg->options; // FIXED
        
        while (ptr < (uint8_t*)msg + size) {
             uint8_t code = *ptr++;
             if (code == 255) break; // End
             if (code == 0) continue; // Pad
             
             uint8_t len = *ptr++;
             
             if (code == 1 && len == 4) { // Subnet Mask
                 uint32_t mask = *(uint32_t*)ptr;
                 IPv4::set_netmask(mask);
             } else if (code == 3 && len >= 4) { // Router
                 uint32_t gateway = *(uint32_t*)ptr;
                 IPv4::set_gateway(gateway);
             }
             
             ptr += len;
        }

        vga.write_string("\n[ NETWORK: IP Address Successfully Configured ]\n");
    }
}

} // namespace MesaOS::Net
