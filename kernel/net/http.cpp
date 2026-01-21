#include "http.hpp"
#include "memory/kheap.hpp"
#include "scheduler.hpp"
#include "logging.hpp"
#include <string.h>
#include "include/string.h" // For itoa

namespace MesaOS::Net {

int HTTP::http_socket = -1;

void HTTP::initialize() {
    // Start HTTP server on port 80
    http_socket = TCP::listen(80);
    if (http_socket >= 0) {
        MesaOS::System::Logging::info("HTTP server started on port 80");
        // Create a process to handle HTTP requests
        MesaOS::System::Scheduler::add_process("httpd", http_server_loop);
    } else {
        MesaOS::System::Logging::error("Failed to start HTTP server on port 80");
    }
}

void HTTP::http_server_loop() {
    for (;;) {
        // Accept incoming connections
        uint32_t client_ip;
        uint16_t client_port;
        int client_socket = TCP::accept(http_socket, &client_ip, &client_port);

        if (client_socket >= 0) {
            // For demonstration, immediately send HTTP response
            // In a real implementation, we'd read and parse the HTTP request
            send_http_response(client_socket);
            TCP::close(client_socket);
        }

        // Yield to other processes
        asm volatile("hlt");
    }
}

void HTTP::send_http_response(int socket_fd) {
    // Create a beautiful HTML page that can be accessed from any browser
    const char* html_content =
        "<!DOCTYPE html>\r\n"
        "<html lang=\"en\">\r\n"
        "<head>\r\n"
        "    <meta charset=\"UTF-8\">\r\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\r\n"
        "    <title>MesaOS - Network Successfully!</title>\r\n"
        "    <style>\r\n"
        "        body {\r\n"
        "            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;\r\n"
        "            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\r\n"
        "            margin: 0;\r\n"
        "            padding: 0;\r\n"
        "            display: flex;\r\n"
        "            justify-content: center;\r\n"
        "            align-items: center;\r\n"
        "            min-height: 100vh;\r\n"
        "            color: white;\r\n"
        "        }\r\n"
        "        .container {\r\n"
        "            text-align: center;\r\n"
        "            background: rgba(255, 255, 255, 0.1);\r\n"
        "            padding: 40px;\r\n"
        "            border-radius: 20px;\r\n"
        "            backdrop-filter: blur(10px);\r\n"
        "            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);\r\n"
        "            border: 1px solid rgba(255, 255, 255, 0.2);\r\n"
        "        }\r\n"
        "        h1 {\r\n"
        "            font-size: 3em;\r\n"
        "            margin-bottom: 20px;\r\n"
        "            text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);\r\n"
        "        }\r\n"
        "        .success {\r\n"
        "            font-size: 1.5em;\r\n"
        "            color: #4CAF50;\r\n"
        "            margin: 20px 0;\r\n"
        "            font-weight: bold;\r\n"
        "        }\r\n"
        "        .info {\r\n"
        "            background: rgba(255, 255, 255, 0.2);\r\n"
        "            padding: 20px;\r\n"
        "            border-radius: 10px;\r\n"
        "            margin: 20px 0;\r\n"
        "        }\r\n"
        "        .services {\r\n"
        "            display: flex;\r\n"
        "            justify-content: space-around;\r\n"
        "            margin-top: 30px;\r\n"
        "        }\r\n"
        "        .service {\r\n"
        "            background: rgba(255, 255, 255, 0.1);\r\n"
        "            padding: 15px;\r\n"
        "            border-radius: 10px;\r\n"
        "            flex: 1;\r\n"
        "            margin: 0 10px;\r\n"
        "        }\r\n"
        "        .service h3 {\r\n"
        "            margin-top: 0;\r\n"
        "            color: #81C784;\r\n"
        "        }\r\n"
        "        .footer {\r\n"
        "            margin-top: 30px;\r\n"
        "            opacity: 0.8;\r\n"
        "            font-size: 0.9em;\r\n"
        "        }\r\n"
        "    </style>\r\n"
        "</head>\r\n"
        "<body>\r\n"
        "    <div class=\"container\">\r\n"
        "        <h1>🚀 MesaOS</h1>\r\n"
        "        <div class=\"success\">✅ Network Successfully!</div>\r\n"
        "        <div class=\"info\">\r\n"
        "            <p><strong>Hello World from MesaOS!</strong></p>\r\n"
        "            <p>This page is being served by a custom HTTP server running on MesaOS - a fully custom operating system written from scratch!</p>\r\n"
        "            <p>You are accessing this from your home network, proving that MesaOS has full TCP/IP networking capabilities.</p>\r\n"
        "        </div>\r\n"
        "        <div class=\"services\">\r\n"
        "            <div class=\"service\">\r\n"
        "                <h3>🌐 HTTP</h3>\r\n"
        "                <p>Port 80 - Web Server</p>\r\n"
        "            </div>\r\n"
        "            <div class=\"service\">\r\n"
        "                <h3>🔐 SSH</h3>\r\n"
        "                <p>Port 22 - Secure Shell</p>\r\n"
        "            </div>\r\n"
        "            <div class=\"service\">\r\n"
        "                <h3>📡 TCP/IP</h3>\r\n"
        "                <p>Full Network Stack</p>\r\n"
        "            </div>\r\n"
        "        </div>\r\n"
        "        <div class=\"footer\">\r\n"
        "            <p>MesaOS v0.6 - Custom Operating System with Networking</p>\r\n"
        "            <p>Built with ❤️ using C++ from scratch</p>\r\n"
        "        </div>\r\n"
        "    </div>\r\n"
        "</body>\r\n"
        "</html>\r\n";

    char header[512];
    int header_len = 0;

    // HTTP/1.1 200 OK
    strcpy(header + header_len, "HTTP/1.1 200 OK\r\n");
    header_len += strlen("HTTP/1.1 200 OK\r\n");

    // Headers
    strcpy(header + header_len, "Content-Type: text/html; charset=UTF-8\r\n");
    header_len += strlen("Content-Type: text/html; charset=UTF-8\r\n");

    strcpy(header + header_len, "Content-Length: ");
    header_len += strlen("Content-Length: ");
    char len_str[16];
    itoa(strlen(html_content), len_str, 10);
    strcpy(header + header_len, len_str);
    header_len += strlen(len_str);
    strcpy(header + header_len, "\r\n");
    header_len += 2;

    strcpy(header + header_len, "Connection: close\r\n");
    header_len += strlen("Connection: close\r\n");

    strcpy(header + header_len, "Server: MesaOS/0.6\r\n");
    header_len += strlen("Server: MesaOS/0.6\r\n");

    strcpy(header + header_len, "\r\n");
    header_len += 2;

    // Send header
    TCP::send(socket_fd, (const uint8_t*)header, header_len);

    // Send HTML content
    TCP::send(socket_fd, (const uint8_t*)html_content, strlen(html_content));
}

void HTTP::handle_request(int socket_fd, const char* request, uint32_t length) {
    (void)socket_fd;
    (void)request;
    (void)length;
    // Parse HTTP request and generate response
    // For now, just send a simple HTML page
}

void HTTP::send_response(int socket_fd, const char* status, const char* content_type, const char* body) {
    char response[2048];
    int len = 0;

    // HTTP header
    strcpy(response + len, "HTTP/1.1 ");
    len += strlen("HTTP/1.1 ");
    strcpy(response + len, status);
    len += strlen(status);
    strcpy(response + len, "\r\n");
    len += 2;

    strcpy(response + len, "Content-Type: ");
    len += strlen("Content-Type: ");
    strcpy(response + len, content_type);
    len += strlen(content_type);
    strcpy(response + len, "\r\n");
    len += 2;

    strcpy(response + len, "Content-Length: ");
    len += strlen("Content-Length: ");
    char len_str[16];
    itoa(strlen(body), len_str, 10);
    strcpy(response + len, len_str);
    len += strlen(len_str);
    strcpy(response + len, "\r\n\r\n");
    len += 4;

    // Body
    strcpy(response + len, body);
    len += strlen(body);

    TCP::send(socket_fd, (const uint8_t*)response, len);
}

} // namespace MesaOS::Net
