//
// Created by Haotian on 2024/10/10.
//

#ifndef SOCKET_POOL_H
#define SOCKET_POOL_H
#include <unordered_map>

#include "../TcpServer.h"

class SocketPool {
    std::unordered_map<socket_type, AsyncSocket *> m_sockets{};
    std::mutex m_mutex{};
public:
    SocketPool() = default;

    ~SocketPool() {
        for (auto &m_socket: m_sockets) {
            delete m_socket.second;
        }
    }

    bool has_socket(const socket_type socket_fd) {
        std::lock_guard lock(m_mutex);
        return m_sockets.count(socket_fd) > 0;
    }

    AsyncSocket *get_or_default(
        const socket_type socket_fd,
        const AsyncSocket::IOType io_type = AsyncSocket::IOType::UNKNOWN) {
        AsyncSocket *socket = nullptr;
        std::lock_guard lock(m_mutex);
        if (m_sockets.count(socket_fd) == 0) {
            socket = new AsyncSocket(io_type, socket_fd);
            m_sockets.emplace(socket_fd, socket);
        } else {
            socket = m_sockets[socket_fd];
        }
        return socket;
    }

    void delete_socket(const socket_type socket_fd) {
        std::lock_guard lock(m_mutex);
        if (m_sockets.count(socket_fd) == 0) {
            const auto socket = m_sockets[socket_fd];
            m_sockets.erase(socket_fd);
            delete socket;
        }
    }
};

#endif //SOCKET_POOL_H
