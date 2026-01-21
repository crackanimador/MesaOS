#ifndef MESAOS_NET_HTTP_HPP
#define MESAOS_NET_HTTP_HPP

#include <stdint.h>
#include "tcp.hpp"

namespace MesaOS::Net {

class HTTP {
public:
    static void initialize();
    static void handle_request(int socket_fd, const char* request, uint32_t length);
    static void send_response(int socket_fd, const char* status, const char* content_type, const char* body);
    static void send_http_response(int socket_fd);
    
private:
    static int http_socket;
    static void http_server_loop();
};

} // namespace MesaOS::Net

#endif // MESAOS_NET_HTTP_HPP
