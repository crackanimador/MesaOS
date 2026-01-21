#include "tcp.hpp"
#include "memory/kheap.hpp"
#include "logging.hpp"
#include <string.h>

namespace MesaOS::Net {

TCPConnection* TCP::connections = nullptr;
uint16_t TCP::next_ephemeral_port = 49152; // Start of ephemeral ports

void TCP::initialize() {
    connections = nullptr;
    next_ephemeral_port = 49152;
}

uint16_t TCP::calculate_checksum(TCPHeader* header, uint32_t src_ip, uint32_t dest_ip, uint32_t length) {
    // Pseudo-header for TCP checksum
    uint32_t sum = 0;
    uint16_t* ptr;

    // Add source IP
    ptr = (uint16_t*)&src_ip;
    sum += ptr[0];
    sum += ptr[1];

    // Add destination IP
    ptr = (uint16_t*)&dest_ip;
    sum += ptr[0];
    sum += ptr[1];

    // Add protocol (6 for TCP) and TCP length
    sum += 6; // Protocol
    sum += length;

    // Add TCP header and data
    ptr = (uint16_t*)header;
    for (uint32_t i = 0; i < length / 2; i++) {
        sum += ptr[i];
    }

    if (length % 2) {
        sum += ((uint8_t*)header)[length - 1];
    }

    // Fold 32-bit sum to 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

void TCP::send_packet(TCPConnection* conn, uint8_t flags, const uint8_t* data, uint32_t data_size) {
    // Check sliding window limits for data packets
    if (data_size > 0 && conn->state == ESTABLISHED) {
        uint32_t available_window = conn->send_window_end - conn->send_window_start;
        if (data_size > available_window) {
            data_size = available_window; // Truncate to fit window
        }
        if (data_size == 0) return; // No space in window
    }

    uint32_t packet_size = sizeof(TCPHeader) + data_size;
    uint8_t* packet = (uint8_t*)kmalloc(packet_size);
    if (!packet) return;

    TCPHeader* header = (TCPHeader*)packet;

    // Fill header
    header->src_port = conn->local_port;
    header->dest_port = conn->remote_port;
    header->seq_number = conn->seq_number;
    header->ack_number = conn->ack_number;
    header->data_offset = (sizeof(TCPHeader) / 4) << 4; // Data offset in 32-bit words
    header->flags = flags;
    header->window_size = conn->advertised_window; // Use our advertised window
    header->checksum = 0;
    header->urgent_ptr = 0;

    // Copy data
    if (data && data_size) {
        memcpy(packet + sizeof(TCPHeader), data, data_size);
    }

    // Calculate checksum
    header->checksum = calculate_checksum(header, conn->local_ip, conn->remote_ip, packet_size);

    // Send via IPv4
    IPv4::send_packet(conn->remote_ip, 6, packet, packet_size);

    kfree(packet);

    // Update sequence number and sliding window
    if (data_size > 0 || (flags & TCP_SYN) || (flags & TCP_FIN)) {
        conn->seq_number += data_size;
        if (flags & TCP_SYN) conn->seq_number++;
        if (flags & TCP_FIN) conn->seq_number++;

        // Update send window for data packets
        if (data_size > 0 && conn->state == ESTABLISHED) {
            conn->send_window_start += data_size;
        }
    }
}

TCPConnection* TCP::find_connection(uint32_t local_ip, uint32_t remote_ip, uint16_t local_port, uint16_t remote_port) {
    TCPConnection* conn = connections;
    while (conn) {
        if (conn->local_ip == local_ip && conn->remote_ip == remote_ip &&
            conn->local_port == local_port && conn->remote_port == remote_port) {
            return conn;
        }
        conn = conn->next;
    }
    return nullptr;
}

TCPConnection* TCP::create_connection(uint32_t local_ip, uint32_t remote_ip, uint16_t local_port, uint16_t remote_port) {
    TCPConnection* conn = (TCPConnection*)kmalloc(sizeof(TCPConnection));
    if (!conn) return nullptr;

    conn->local_ip = local_ip;
    conn->remote_ip = remote_ip;
    conn->local_port = local_port;
    conn->remote_port = remote_port;
    conn->state = CLOSED;
    conn->seq_number = 0x12345678; // Random-ish starting sequence
    conn->ack_number = 0;
    conn->window_size = TCP_DEFAULT_WINDOW_SIZE;
    conn->advertised_window = TCP_DEFAULT_WINDOW_SIZE;

    // Initialize sliding window
    conn->send_window_start = conn->seq_number;
    conn->send_window_end = conn->seq_number + TCP_DEFAULT_WINDOW_SIZE;
    conn->recv_window_start = 0;
    conn->recv_window_end = TCP_DEFAULT_WINDOW_SIZE;

    // Initialize receive buffer
    conn->recv_start = 0;
    conn->recv_end = 0;
    conn->recv_count = 0;

    // Initialize retransmission
    conn->last_ack_time = 0;
    conn->rto = TCP_RETRANSMIT_TIMEOUT_MS;
    conn->retransmit_count = 0;

    // Initialize SYN flood protection
    conn->syn_timestamp = 0;
    conn->syn_cookie_enabled = true;

    conn->next = connections;
    connections = conn;

    return conn;
}

void TCP::remove_connection(TCPConnection* conn) {
    if (!conn) return;

    if (connections == conn) {
        connections = conn->next;
    } else {
        TCPConnection* prev = connections;
        while (prev && prev->next != conn) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = conn->next;
        }
    }

    kfree(conn);
}

void TCP::handle_packet(uint32_t src_ip, uint8_t* data, uint32_t size) {
    if (size < sizeof(TCPHeader)) return;

    TCPHeader* header = (TCPHeader*)data;
    uint32_t data_offset = (header->data_offset >> 4) * 4;
    uint32_t data_size = size - data_offset;
    uint8_t* payload = data + data_offset;

    TCPConnection* conn = find_connection(IPv4::get_ip(), src_ip, header->dest_port, header->src_port);

    if (!conn) {
        // Check if it's a SYN to a listening socket
        // For now, we'll create connections on demand
        if (header->flags & TCP_SYN) {
            // SYN flood protection: Check if we've seen too many SYNs recently
            static uint32_t syn_count = 0;
            static uint32_t last_syn_time = 0;
            uint32_t current_time = 0; // TODO: Get actual timestamp

            if (current_time - last_syn_time < 1000) { // Within 1 second
                syn_count++;
                if (syn_count > 10) { // More than 10 SYNs per second
                    MesaOS::System::Logging::warn("SYN flood detected - dropping SYN packet");
                    return; // Drop the packet
                }
            } else {
                syn_count = 1;
                last_syn_time = current_time;
            }

            conn = create_connection(IPv4::get_ip(), src_ip, header->dest_port, header->src_port);
            if (conn) {
                conn->state = LISTEN;
                conn->syn_timestamp = current_time; // Record when SYN was received
            }
        }
    }

    if (!conn) return;

    // Update acknowledgment number
    conn->ack_number = header->seq_number + data_size;
    if (header->flags & TCP_SYN) conn->ack_number++;
    if (header->flags & TCP_FIN) conn->ack_number++;

    // Handle acknowledgments
    if (header->flags & TCP_ACK) {
        // Acknowledgment received - could implement more advanced ACK handling here
    }

    // Handle different states
    switch (conn->state) {
        case LISTEN:
            if (header->flags & TCP_SYN) {
                // Send SYN-ACK
                conn->state = SYN_RECEIVED;
                send_packet(conn, TCP_SYN | TCP_ACK, nullptr, 0);
            }
            break;

        case SYN_SENT:
            if ((header->flags & TCP_SYN) && (header->flags & TCP_ACK)) {
                // Connection established
                conn->state = ESTABLISHED;
                send_packet(conn, TCP_ACK, nullptr, 0);
            }
            break;

        case SYN_RECEIVED:
            if (header->flags & TCP_ACK) {
                // Connection fully established
                conn->state = ESTABLISHED;
            }
            break;

        case ESTABLISHED:
            if (header->flags & TCP_FIN) {
                conn->state = CLOSE_WAIT;
                send_packet(conn, TCP_ACK, nullptr, 0);
                // Send our FIN
                conn->state = LAST_ACK;
                send_packet(conn, TCP_FIN | TCP_ACK, nullptr, 0);
            } else if (data_size > 0) {
                // Add data to receive buffer
                uint32_t space_available = TCP_BUFFER_SIZE - conn->recv_count;
                uint32_t bytes_to_copy = (data_size < space_available) ? data_size : space_available;

                for (uint32_t i = 0; i < bytes_to_copy; i++) {
                    conn->recv_buffer[conn->recv_end] = payload[i];
                    conn->recv_end = (conn->recv_end + 1) % TCP_BUFFER_SIZE;
                }
                conn->recv_count += bytes_to_copy;

                // Send acknowledgment
                send_packet(conn, TCP_ACK, nullptr, 0);
            } else if (header->flags & TCP_ACK) {
                // Acknowledgment received
            }
            break;

        case FIN_WAIT_1:
            if (header->flags & TCP_ACK) {
                conn->state = FIN_WAIT_2;
            }
            break;

        case FIN_WAIT_2:
            if (header->flags & TCP_FIN) {
                conn->state = TIME_WAIT;
                send_packet(conn, TCP_ACK, nullptr, 0);
            }
            break;

        case CLOSE_WAIT:
            // Waiting for application to close
            break;

        case LAST_ACK:
            if (header->flags & TCP_ACK) {
                remove_connection(conn);
            }
            break;

        case TIME_WAIT:
            // Clean up after timeout (simplified)
            remove_connection(conn);
            break;

        default:
            break;
    }
}

// Server functions
int TCP::listen(uint16_t port) {
    TCPConnection* conn = create_connection(IPv4::get_ip(), 0, port, 0);
    if (!conn) {
        MesaOS::System::Logging::error("TCP listen failed - could not create connection");
        return -1;
    }

    conn->state = LISTEN;
    char log_msg[64];
    strcpy(log_msg, "TCP listening on port ");
    char port_str[8];
    itoa(port, port_str, 10);
    strcat(log_msg, port_str);
    MesaOS::System::Logging::info(log_msg);
    return (int)conn; // Return connection as socket descriptor
}

int TCP::accept(int socket_fd, uint32_t* client_ip, uint16_t* client_port) {
    TCPConnection* conn = (TCPConnection*)socket_fd;
    if (!conn || conn->state != ESTABLISHED) return -1;

    if (client_ip) *client_ip = conn->remote_ip;
    if (client_port) *client_port = conn->remote_port;

    return socket_fd;
}

int TCP::recv(int socket_fd, uint8_t* buffer, uint32_t size) {
    TCPConnection* conn = (TCPConnection*)socket_fd;
    if (!conn || conn->state != ESTABLISHED || !buffer) return -1;

    if (conn->recv_count == 0) return 0; // No data available

    uint32_t bytes_to_read = (size < conn->recv_count) ? size : conn->recv_count;

    for (uint32_t i = 0; i < bytes_to_read; i++) {
        buffer[i] = conn->recv_buffer[conn->recv_start];
        conn->recv_start = (conn->recv_start + 1) % TCP_BUFFER_SIZE;
    }
    conn->recv_count -= bytes_to_read;

    return bytes_to_read;
}

int TCP::send(int socket_fd, const uint8_t* data, uint32_t size) {
    TCPConnection* conn = (TCPConnection*)socket_fd;
    if (!conn || conn->state != ESTABLISHED) return -1;

    send_packet(conn, TCP_ACK | TCP_PSH, data, size);
    return size;
}

void TCP::close(int socket_fd) {
    TCPConnection* conn = (TCPConnection*)socket_fd;
    if (!conn) return;

    if (conn->state == ESTABLISHED) {
        conn->state = FIN_WAIT_1;
        send_packet(conn, TCP_FIN | TCP_ACK, nullptr, 0);
    } else {
        remove_connection(conn);
    }
}

// Client functions
int TCP::connect(uint32_t dest_ip, uint16_t dest_port) {
    uint16_t local_port = next_ephemeral_port++;
    TCPConnection* conn = create_connection(IPv4::get_ip(), dest_ip, local_port, dest_port);
    if (!conn) {
        MesaOS::System::Logging::error("TCP connect: Failed to create connection");
        return -1;
    }

    // Send SYN packet to initiate connection
    conn->state = SYN_SENT;
    MesaOS::System::Logging::info("TCP connect: Sending SYN packet");
    send_packet(conn, TCP_SYN, nullptr, 0);

    // In a real TCP implementation, we would wait here for SYN-ACK
    // For now, we simulate the connection establishment
    // The handle_packet function will process the SYN-ACK when it arrives

    // For testing purposes, we'll assume the handshake completes
    // In practice, this should be handled asynchronously
    conn->state = ESTABLISHED;
    MesaOS::System::Logging::info("TCP connect: Connection established (handshake simulated)");
    return (int)conn;
}

void TCP::handle_packet_ipv6(const uint8_t* src_ip, uint8_t* data, uint32_t size) {
    // Placeholder for IPv6 TCP handling
    // TODO: Implement full IPv6 TCP support
    MesaOS::System::Logging::info("IPv6 TCP packet received (not implemented yet)");
    (void)src_ip;
    (void)data;
    (void)size;
}



} // namespace MesaOS::Net
