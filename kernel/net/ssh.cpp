#include "ssh.hpp"
#include "memory/kheap.hpp"
#include "scheduler.hpp"
#include <string.h>

namespace MesaOS::Net {

int SSH::ssh_socket = -1;
static int current_ssh_client_socket = -1; // Global for lambda

// Simple authentication - in real SSH, this would use proper crypto
const char* SSH_USERS[][2] = {
    {"mesa", "password"},
    {"root", "rootpass"},
    {nullptr, nullptr}
};

void SSH::initialize() {
    // Start SSH server on port 22
    ssh_socket = TCP::listen(22);
    if (ssh_socket >= 0) {
        // Create a process to handle SSH connections
        MesaOS::System::Scheduler::add_process("sshd", ssh_server_loop);
    }
}

static void ssh_session_handler() {
    const char* banner = "SSH-2.0-MesaOS_1.0\r\n";
    TCP::send(current_ssh_client_socket, (const uint8_t*)banner, strlen(banner));
    TCP::close(current_ssh_client_socket);
}

void SSH::ssh_server_loop() {
    for (;;) {
        // Accept incoming connections
        uint32_t client_ip;
        uint16_t client_port;
        int client_socket = TCP::accept(ssh_socket, &client_ip, &client_port);

        if (client_socket >= 0) {
            // Handle SSH connection in a new process
            current_ssh_client_socket = client_socket;
            MesaOS::System::Scheduler::add_process("ssh_session", ssh_session_handler);
        }

        // Yield to other processes
        asm volatile("hlt");
    }
}

void SSH::handle_connection(int socket_fd) {
    // Send SSH version string
    const char* version = "SSH-2.0-MesaOS_SSH_1.0\r\n";
    TCP::send(socket_fd, (const uint8_t*)version, strlen(version));
    
    // In a full implementation, we'd handle key exchange, authentication, etc.
    // For now, just close the connection
    TCP::close(socket_fd);
}

bool SSH::authenticate_user(int socket_fd, const char* username, const char* password) {
    (void)socket_fd;
    
    for (int i = 0; SSH_USERS[i][0] != nullptr; i++) {
        if (strcmp(username, SSH_USERS[i][0]) == 0 &&
            strcmp(password, SSH_USERS[i][1]) == 0) {
            return true;
        }
    }
    return false;
}

void SSH::handle_ssh_session(int socket_fd) {
    // Simplified SSH session handling
    const char* welcome = "\r\nWelcome to MesaOS SSH Server\r\n";
    TCP::send(socket_fd, (const uint8_t*)welcome, strlen(welcome));
    
    // Close after welcome message
    TCP::close(socket_fd);
}

void SSH::send_ssh_packet(int socket_fd, uint8_t type, const uint8_t* data, uint32_t length) {
    // SSH packet format: length (4 bytes) + padding length (1 byte) + type (1 byte) + data + padding
    uint8_t packet[4096];
    uint32_t packet_length = 1 + length; // type + data
    uint8_t padding_length = 8 - (packet_length % 8);
    if (padding_length < 4) padding_length += 8;
    
    uint32_t total_length = 4 + 1 + packet_length + padding_length;
    
    // Length
    packet[0] = (total_length - 4) >> 24;
    packet[1] = (total_length - 4) >> 16;
    packet[2] = (total_length - 4) >> 8;
    packet[3] = (total_length - 4);
    
    // Padding length
    packet[4] = padding_length;
    
    // Type
    packet[5] = type;
    
    // Data
    if (data && length) {
        memcpy(packet + 6, data, length);
    }
    
    // Padding (zeros for simplicity)
    memset(packet + 6 + length, 0, padding_length);
    
    TCP::send(socket_fd, packet, total_length);
}

} // namespace MesaOS::Net
