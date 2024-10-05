//
// Created by Haotian on 2024/10/1.
//

#include "TcpServer.h"

#include "MultiplexingWindows.h"

// static void close_socket(const socket_type fd) {
// #ifdef WINDOWS
//     closesocket(fd);
// #endif
// #ifdef LINUX
//     close(fd);
// #endif
// }
//
// static long receive_bytes(const socket_type fd, char *buf, const int buf_size) {
// #ifdef WINDOWS
//     return recv(fd, buf, buf_size, 0);
// #endif
// #ifdef LINUX
//     return read(fd, buf, buf_size);
// #endif
// }

void TcpServer::exit_with_error(const std::string &message) const {
    logger->error(message);
    exit(-1);
}

void TcpServer::on_connected(const ConnectionBehavior &connection) {
    behavior = connection;
}

TcpServer::TcpServer(std::string &&ip_address, const int ip_port)
    : socket_address(),
      ip_address(std::move(ip_address)),
      ip_port(ip_port) {
    socket_fd = -1;
    accept_socket_fd = -1;
    logger = Logger::get_logger();
    // ipv4
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(ip_port);
    socket_address.sin_addr.s_addr = inet_addr(this->ip_address.c_str());
}

int TcpServer::start_server() {
#ifdef WINDOWS
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) {
        exit_with_error("WSAStartup failed");
    }
    socket_fd = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, nullptr, 0, WSA_FLAG_OVERLAPPED);

    multiplexing = new MultiplexingWindows(socket_fd);
    multiplexing->on_connected(std::move(behavior));
#elif defined(LINUX)
    // Create the socket with ipv4 and automatic protocol
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
#endif
    if (socket_fd < 0) {
        exit_with_error("Failed to create socket.");
    }
    // Bind the socket to ip address
    if (bind(socket_fd,
             reinterpret_cast<sockaddr *>(&socket_address),
             sizeof(socket_address))) {
        exit_with_error("Failed to bind socket.");
    }
    start_listening();

    multiplexing->setup();
    multiplexing->start();
    return 0;
}

void TcpServer::close_server() {
    if (is_shutdown) return;
    is_shutdown = true;
    multiplexing->stop();
#ifdef WINDOWS
    WSACleanup();
#endif
    close_socket(socket_fd);
}

void TcpServer::start_listening() const {
    if (listen(socket_fd, MAX_CONNECTIONS) < 0) {
        exit_with_error("Failed to listen on socket.");
    }
    std::ostringstream ss;
    ss << "\n*** Listening on ADDRESS: "
            << inet_ntoa(socket_address.sin_addr)
            << " PORT: " << ntohs(socket_address.sin_port)
            << " ***"
            << std::endl;
    logger->log(ss.str());
}

void TcpServer::accept_connection() {
    socket_len_type size = sizeof(socket_address);

    while (true) {
        char buffer[BUFFER_SIZE] = {0}; {
            std::ostringstream ss;
            ss << "====== Waiting for a new connection ======"
                    << std::endl;
            logger->log(ss.str());
        }

        accept_socket_fd = accept(socket_fd,
                                  reinterpret_cast<sockaddr *>(&socket_address),
                                  &size);
        if (accept_socket_fd < 0) {
            return;
        }

        const auto bytes_received = receive_bytes(accept_socket_fd, buffer, BUFFER_SIZE);
        if (bytes_received < 0) {
            logger->error("Failed to read bytes from client socket connection");
        } {
            std::stringstream ss;
            ss << "------ Received Request from client ------\n";
            ss << "Received bytes: " << bytes_received << "\n";
            ss << buffer;
            logger->log(ss.str());
            if (strcmp(buffer, "exit") == 0
                || strcmp(buffer, "exit\n") == 0
                || strcmp(buffer, "exit\r\n") == 0) {
                close_socket(accept_socket_fd);
                close_server();
                exit(0);
            }
        }

        close_socket(accept_socket_fd);
    }
}
