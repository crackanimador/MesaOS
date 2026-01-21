#ifndef MESAOS_NET_TCP_HPP
#define MESAOS_NET_TCP_HPP

#include <stdint.h>
#include "ipv4.hpp"

namespace MesaOS::Net {

#pragma pack(push, 1)
struct TCPHeader {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_number;
    uint32_t ack_number;
    uint8_t data_offset; // 4 bits
    uint8_t flags;       // 6 bits + 2 reserved
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
    uint8_t options[0]; // Variable length
};
#pragma pack(pop)

// TCP flags
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20

// TCP states
enum TCPState {
    CLOSED,
    LISTEN,
    SYN_SENT,
    SYN_RECEIVED,
    ESTABLISHED,
    FIN_WAIT_1,
    FIN_WAIT_2,
    CLOSE_WAIT,
    CLOSING,
    LAST_ACK,
    TIME_WAIT
};

/**
 * TCP Connection Structure with Sliding Window and Robustness Features
 *
 * This enhanced TCP implementation includes:
 * - Sliding window for flow control and congestion avoidance
 * - Retransmission timeout handling for reliability
 * - SYN flood protection to prevent DoS attacks
 * - Proper buffer management for large data transfers
 */
#define TCP_BUFFER_SIZE 4096
#define TCP_MAX_WINDOW_SIZE 65535
#define TCP_DEFAULT_WINDOW_SIZE 8192
#define TCP_RETRANSMIT_TIMEOUT_MS 1000
#define TCP_MAX_RETRIES 5

struct TCPConnection {
    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    TCPState state;
    uint32_t seq_number;
    uint32_t ack_number;
    uint32_t window_size;
    uint32_t advertised_window; // Our advertised window

    // Sliding window implementation for reliable data transfer
    uint32_t send_window_start;  // First unacknowledged byte
    uint32_t send_window_end;    // Last byte we can send
    uint32_t recv_window_start;  // Next expected byte
    uint32_t recv_window_end;    // Last byte in receive window

    // Receive buffer for incoming data
    uint8_t recv_buffer[TCP_BUFFER_SIZE];
    uint32_t recv_start;
    uint32_t recv_end;
    uint32_t recv_count;

    // Retransmission handling for lost packets
    uint32_t last_ack_time;      // Timestamp of last ACK
    uint32_t rto;                // Retransmission timeout
    uint8_t retransmit_count;    // Number of retransmits

    // SYN flood protection mechanisms
    uint32_t syn_timestamp;      // When SYN was received
    bool syn_cookie_enabled;     // Use SYN cookies for protection

    TCPConnection* next;
};

class TCP {
public:
    static void initialize();
    static void handle_packet(uint32_t src_ip, uint8_t* data, uint32_t size);
    static void handle_packet_ipv6(const uint8_t* src_ip, uint8_t* data, uint32_t size); // Placeholder
    
    // Server functions
    static int listen(uint16_t port);
    static int accept(int socket_fd, uint32_t* client_ip, uint16_t* client_port);
    static int recv(int socket_fd, uint8_t* buffer, uint32_t size);
    static int send(int socket_fd, const uint8_t* data, uint32_t size);
    static void close(int socket_fd);
    
    // Client functions
    static int connect(uint32_t dest_ip, uint16_t dest_port);

private:
    static TCPConnection* connections;
    static uint16_t next_ephemeral_port;
    
    static uint16_t calculate_checksum(TCPHeader* header, uint32_t src_ip, uint32_t dest_ip, uint32_t length);
    static void send_packet(TCPConnection* conn, uint8_t flags, const uint8_t* data, uint32_t data_size);
    static TCPConnection* find_connection(uint32_t local_ip, uint32_t remote_ip, uint16_t local_port, uint16_t remote_port);
    static TCPConnection* create_connection(uint32_t local_ip, uint32_t remote_ip, uint16_t local_port, uint16_t remote_port);
    static void remove_connection(TCPConnection* conn);
};

} // namespace MesaOS::Net

#endif // MESAOS_NET_TCP_HPP
