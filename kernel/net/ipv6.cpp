#include "ipv6.hpp"
#include "ethernet.hpp"
#include "tcp.hpp"
#include "udp.hpp"
#include "memory/kheap.hpp"
#include "logging.hpp"
#include <string.h>

namespace MesaOS::Net {

uint8_t IPv6::link_local_addr[16] = {0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}; // fe80::1
uint8_t IPv6::global_addr[16] = {0};
bool IPv6::has_global_addr = false;

// Neighbor cache
uint8_t IPv6::neighbor_cache[8][6];
uint8_t IPv6::neighbor_addrs[8][16];
uint8_t IPv6::neighbor_count = 0;

void IPv6::initialize() {
    // Generate link-local address based on MAC address
    const uint8_t* mac = Ethernet::get_mac_address();
    if (mac) {
        // Copy MAC to EUI-64 format
        link_local_addr[8] = mac[0] ^ 0x02;  // Flip universal/local bit
        link_local_addr[9] = mac[1];
        link_local_addr[10] = mac[2];
        link_local_addr[11] = 0xff;
        link_local_addr[12] = 0xfe;
        link_local_addr[13] = mac[3];
        link_local_addr[14] = mac[4];
        link_local_addr[15] = mac[5];
    }

    MesaOS::System::Logging::info("IPv6 initialized");
}

void IPv6::handle_packet(uint8_t* data, uint32_t size) {
    if (size < sizeof(IPv6Header)) return;

    IPv6Header* header = (IPv6Header*)data;

    // Check version
    uint8_t version = (header->version_traffic_flow >> 28) & 0xF;
    if (version != IPV6_VERSION) return;

    uint16_t payload_length = __builtin_bswap16(header->payload_length);
    uint8_t next_header = header->next_header;
    uint8_t* payload = data + sizeof(IPv6Header);

    // Check if destination is us
    bool is_for_us = false;
    if (memcmp(header->dest_addr, link_local_addr, 16) == 0) is_for_us = true;
    if (has_global_addr && memcmp(header->dest_addr, global_addr, 16) == 0) is_for_us = true;

    if (!is_for_us) {
        // TODO: Forwarding not implemented yet
        return;
    }

    // Route to appropriate protocol handler
    if (next_header == 6) { // TCP
        TCP::handle_packet_ipv6(header->src_addr, payload, payload_length);
    } else if (next_header == 17) { // UDP
        UDP::handle_packet_ipv6(header->src_addr, payload, payload_length);
    } else if (next_header == 58) { // ICMPv6
        handle_icmpv6_packet(header->src_addr, payload, payload_length);
    } else {
        MesaOS::System::Logging::info("IPv6 unknown next_header");
    }
}

void IPv6::send_packet(const uint8_t* dest_addr, uint8_t next_header, const uint8_t* payload, uint32_t payload_size) {
    uint32_t packet_size = sizeof(IPv6Header) + payload_size;
    uint8_t* packet = (uint8_t*)kmalloc(packet_size);
    if (!packet) return;

    IPv6Header* header = (IPv6Header*)packet;

    // Fill IPv6 header
    header->version_traffic_flow = (IPV6_VERSION << 28) | (0 << 20) | 0; // Version 6, no traffic class/flow label
    header->payload_length = __builtin_bswap16(payload_size);
    header->next_header = next_header;
    header->hop_limit = 64; // Default hop limit

    // Set source address
    const uint8_t* src_addr = has_global_addr ? global_addr : link_local_addr;
    memcpy(header->src_addr, src_addr, 16);

    // Set destination address
    memcpy(header->dest_addr, dest_addr, 16);

    // Copy payload
    if (payload && payload_size) {
        memcpy(packet + sizeof(IPv6Header), payload, payload_size);
    }

    // Send via Ethernet
    // TODO: Implement proper neighbor discovery for IPv6
    // For now, send to broadcast (simplified)
    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    Ethernet::send_packet(broadcast_mac, 0x86DD, packet, packet_size);

    kfree(packet);
}

const uint8_t* IPv6::get_link_local_addr() {
    return link_local_addr;
}

void IPv6::set_global_addr(const uint8_t* addr) {
    memcpy(global_addr, addr, 16);
    has_global_addr = true;
}

uint16_t IPv6::calculate_checksum(const IPv6Header* header, const uint8_t* payload, uint32_t payload_size) {
    // IPv6 doesn't use header checksum like IPv4
    // Upper layer protocols calculate their own checksums
    (void)header;
    (void)payload;
    (void)payload_size;
    return 0;
}

void IPv6::handle_icmpv6_packet(const uint8_t* src_addr, uint8_t* data, uint32_t size) {
    if (size < 4) return; // Minimum ICMPv6 header size

    uint8_t type = data[0];
    uint8_t code = data[1];

    switch (type) {
        case 135: // Neighbor Solicitation
            if (size >= 24) { // NS header + target address
                uint8_t* target_addr = data + 8; // Skip reserved field

                // Check if target address is ours
                bool is_ours = false;
                if (memcmp(target_addr, link_local_addr, 16) == 0) is_ours = true;
                if (has_global_addr && memcmp(target_addr, global_addr, 16) == 0) is_ours = true;

                if (is_ours) {
                    // Send Neighbor Advertisement
                    send_neighbor_advertisement(src_addr, target_addr);
                }

                // Update neighbor cache with source address
                // TODO: Get MAC from Ethernet frame
                // update_neighbor_cache(src_addr, mac_from_frame);
            }
            break;

        case 136: // Neighbor Advertisement
            if (size >= 24) { // NA header + target address
                uint8_t* target_addr = data + 8;

                // Update neighbor cache
                // TODO: Get MAC from Ethernet frame
                // update_neighbor_cache(target_addr, mac_from_frame);
            }
            break;

        default:
            MesaOS::System::Logging::info("ICMPv6 unhandled type");
            break;
    }
}

void IPv6::send_neighbor_advertisement(const uint8_t* dest_addr, const uint8_t* target_addr) {
    uint32_t payload_size = 24; // Header + target addr + source link addr option
    uint8_t* payload = (uint8_t*)kmalloc(payload_size);
    if (!payload) return;

    // Neighbor Advertisement header
    payload[0] = 136; // Type: Neighbor Advertisement
    payload[1] = 0;   // Code
    payload[2] = 0;   // Checksum (calculated later)
    payload[3] = 0;
    payload[4] = 0x60; // Flags: Solicited + Override
    payload[5] = 0;
    payload[6] = 0;
    payload[7] = 0;

    // Target address
    memcpy(payload + 8, target_addr, 16);

    // Source Link Address option (Type 2)
    payload[24] = 2;  // Option type
    payload[25] = 1;  // Length (8 octets)
    memcpy(payload + 26, Ethernet::get_mac_address(), 6);

    // Calculate checksum (simplified - should use proper IPv6 pseudo-header)
    uint16_t checksum = 0;
    for (uint32_t i = 0; i < payload_size; i += 2) {
        checksum += (payload[i] << 8) | payload[i + 1];
    }
    payload[2] = (checksum >> 8) & 0xFF;
    payload[3] = checksum & 0xFF;

    send_packet(dest_addr, 58, payload, payload_size);
    kfree(payload);

    MesaOS::System::Logging::info("Sent Neighbor Advertisement");
}

void IPv6::update_neighbor_cache(const uint8_t* ip_addr, const uint8_t* mac_addr) {
    if (!ip_addr || !mac_addr) return;

    // Look for existing entry
    for (uint8_t i = 0; i < neighbor_count; ++i) {
        if (memcmp(neighbor_addrs[i], ip_addr, 16) == 0) {
            // Update existing entry
            memcpy(neighbor_cache[i], mac_addr, 6);
            return;
        }
    }

    // Add new entry if space available
    if (neighbor_count < 8) {
        memcpy(neighbor_addrs[neighbor_count], ip_addr, 16);
        memcpy(neighbor_cache[neighbor_count], mac_addr, 6);
        neighbor_count++;
    }
}



} // namespace MesaOS::Net
