//
// Created by Haotian on 2024/10/1.
//

#ifndef TCPSERVER_H
#define TCPSERVER_H
#include "Multiplexing.h"
#include "../common/Predefined.h"

#ifdef WINDOWS
#include <winsock2.h>
typedef SOCKET socket_type;
typedef int socket_len_type;
#endif

#ifdef LINUX
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int socket_type;
typedef socklen_t socket_len_type;
#endif

#include <memory>
#include <sstream>
#include <cstring>
#include "../log/Logger.h"

class TcpServer {
    // Only for test
    static constexpr int MAX_CONNECTIONS = 20000;
    static constexpr size_t BUFFER_SIZE = 30720;

    Logger *m_logger;

    // socket file descriptor
    socket_type m_listen_socket = -1, m_accept_socket_fd = -1;
    // socket info
    sockaddr_in m_socket_address;
    // ip address
    const std::string m_ip_address;
    // port
    const int m_ip_port;

    // If the os is Linux, use epoll; If Windows, use IOCP.
    Multiplexing *m_multiplexing = nullptr;

    // Send, Receive Callback
    ConnectionBehavior m_behavior{};

    bool m_is_shutdown = false;

    // Create sockets, bind port, and more
    void setup();

    void start_listening() const;

    void exit_with_error(const std::string &message) const;

public:
    explicit TcpServer(std::string &&ip_address = "0.0.0.0", int ip_port = 8080);

    // Bind callback
    void set_callback(const ConnectionBehavior &behavior);

    int start_server();

    void close_server();

    // Only for test
    void accept_connection();
};


#endif //TCPSERVER_H
