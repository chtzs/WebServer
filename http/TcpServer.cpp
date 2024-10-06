//
// Created by Haotian on 2024/10/1.
//

#include "TcpServer.h"

#include "MultiplexingLinux.h"
#include "MultiplexingWindows.h"

void TcpServer::exit_with_error(const std::string &message) const {
    m_logger->error(message.c_str());
    exit(-1);
}

void TcpServer::set_callback(const ConnectionBehavior &behavior) {
    m_behavior = behavior;
}

TcpServer::TcpServer(std::string &&ip_address, const int ip_port)
    : m_socket_address(),
      m_ip_address(std::move(ip_address)),
      m_ip_port(ip_port) {
    m_listen_socket = -1;
    m_accept_socket_fd = -1;
    m_logger = Logger::get_logger();
    // ipv4
    m_socket_address.sin_family = AF_INET;
    m_socket_address.sin_port = htons(ip_port);
    m_socket_address.sin_addr.s_addr = inet_addr(this->m_ip_address.c_str());
}

int TcpServer::start_server() {
    setup();
    start_listening();

    m_multiplexing->setup();
    m_multiplexing->start();

    return 0;
}

void TcpServer::close_server() {
    if (m_is_shutdown) return;
    m_is_shutdown = true;
    m_multiplexing->stop();
#ifdef WINDOWS
    WSACleanup();
#endif
    close_socket(m_listen_socket);
}

void TcpServer::setup() {
#ifdef WINDOWS
    // Require Windows Socket Api 2.0
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) {
        exit_with_error("WSAStartup failed");
    }
    // Create listen socket with IOCP(WSA_FLAG_OVERLAPPED)
    m_listen_socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, nullptr, 0, WSA_FLAG_OVERLAPPED);

    m_multiplexing = new MultiplexingWindows(m_listen_socket);
    m_multiplexing->set_callback(m_behavior);
#elif defined(LINUX)
    // Create the socket with ipv4 and automatic protocol
    m_listen_socket = socket(AF_INET, SOCK_STREAM, 0);

    m_multiplexing = new MultiplexingLinux(m_listen_socket);
    m_multiplexing->set_callback(m_behavior);
#endif
    if (m_listen_socket < 0) {
        exit_with_error("Failed to create socket.");
    }
    // Bind the socket to ip address
    if (bind(m_listen_socket,
             reinterpret_cast<sockaddr *>(&m_socket_address),
             sizeof(m_socket_address))) {
        exit_with_error("Failed to bind socket.");
    }
}

void TcpServer::start_listening() const {
    if (listen(m_listen_socket, MAX_CONNECTIONS) < 0) {
        exit_with_error("Failed to listen on socket.");
    }
    std::ostringstream ss;
    ss << "\n*** Listening on ADDRESS: "
            << inet_ntoa(m_socket_address.sin_addr)
            << " PORT: " << ntohs(m_socket_address.sin_port)
            << " ***"
            << std::endl;
    m_logger->log(ss.str().c_str());
}

void TcpServer::accept_connection() {
    socket_len_type size = sizeof(m_socket_address);

    while (true) {
        char buffer[BUFFER_SIZE] = {0}; {
            std::ostringstream ss;
            ss << "====== Waiting for a new connection ======"
                    << std::endl;
            m_logger->log(ss.str().c_str());
        }

        m_accept_socket_fd = accept(m_listen_socket,
                                  reinterpret_cast<sockaddr *>(&m_socket_address),
                                  &size);
        if (m_accept_socket_fd < 0) {
            return;
        }

        const auto bytes_received = receive_bytes(m_accept_socket_fd, buffer, BUFFER_SIZE);
        if (bytes_received < 0) {
            m_logger->error("Failed to read bytes from client socket connection");
        } {
            std::stringstream ss;
            ss << "------ Received Request from client ------\n";
            ss << "Received bytes: " << bytes_received << "\n";
            ss << buffer;
            m_logger->log(ss.str().c_str());
            if (strcmp(buffer, "exit") == 0
                || strcmp(buffer, "exit\n") == 0
                || strcmp(buffer, "exit\r\n") == 0) {
                close_socket(m_accept_socket_fd);
                close_server();
                exit(0);
            }
        }

        close_socket(m_accept_socket_fd);
    }
}
