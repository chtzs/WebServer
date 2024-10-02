//
// Created by Haotian on 2024/10/1.
//

#ifndef TCPSERVER_H
#define TCPSERVER_H
#include "../common/Predefined.h"

#ifdef WINDOWS
#include <winsock.h>
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
    // 常量
    static constexpr int MAX_CONNECTIONS = 20;
    static constexpr size_t BUFFER_SIZE = 30720;
    Logger *logger;

    // socket file descriptor
    socket_type socket_fd, accept_socket_fd;
    // socket info
    sockaddr_in socket_address;
    // ip address
    const std::string ip_address;
    // port
    const int ip_port;

    void start_listening() const;

    void exit_with_error(const std::string &message) const;

public:
    explicit TcpServer(std::string &&ip_address = "0.0.0.0", int ip_port = 8080);

    int start_server();

    void close_server() const;

    void accept_connection();
};


#endif //TCPSERVER_H
