#ifndef MESAOS_NET_SSH_HPP
#define MESAOS_NET_SSH_HPP

#include <stdint.h>
#include "tcp.hpp"

namespace MesaOS::Net {

class SSH {
public:
    static void initialize();
    static void handle_connection(int socket_fd);
    
private:
    static int ssh_socket;
    static void ssh_server_loop();
    static bool authenticate_user(int socket_fd, const char* username, const char* password);
    static void handle_ssh_session(int socket_fd);
    static void send_ssh_packet(int socket_fd, uint8_t type, const uint8_t* data, uint32_t length);
};

} // namespace MesaOS::Net

#endif // MESAOS_NET_SSH_HPP
