//
// Created by Haotian on 2024/10/2.
//

#ifndef MULTIPLEXING_H
#define MULTIPLEXING_H

#include <functional>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "../../common/Predefined.h"
#include "../../common/SafeQueue.h"

#ifdef LINUX
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#endif

static void close_socket(const socket_type fd) {
#ifdef WINDOWS
    closesocket(fd);
#endif
#ifdef LINUX
    close(fd);
#endif
}

static long receive_bytes(const socket_type fd, char *buf, const int buf_size) {
#ifdef WINDOWS
    return recv(fd, buf, buf_size, 0);
#endif
#ifdef LINUX
    return read(fd, buf, buf_size);
#endif
}

struct SocketBuffer {
    static constexpr ssize_t MAX_SIZE = 2048;
#ifdef WINDOWS
    WSABUF wsaBuf{MAX_SIZE, buffer};
#endif
    char buffer[MAX_SIZE]{};
    ssize_t size = 0;
    char *p_current = buffer;

    char operator[](const int index) const {
        return buffer[index];
    }

    SocketBuffer() = default;

    SocketBuffer(const SocketBuffer &other) {
        memcpy(buffer, other.buffer, sizeof(buffer));
#ifdef WINDOWS
        wsaBuf.buf = buffer;
        wsaBuf.len = other.wsaBuf.len;
#endif
        p_current = buffer;
        size = other.size;
    }

    SocketBuffer &operator=(const SocketBuffer &other) {
        if (this != &other) {
            memcpy(buffer, other.buffer, sizeof(buffer));
#ifdef WINDOWS
            wsaBuf.buf = buffer;
            wsaBuf.len = other.wsaBuf.len;
#endif
            p_current = buffer;
            size = other.size;
        }
        return *this;
    }
};

#ifdef WINDOWS

// struct AsyncSocket {
//     OVERLAPPED overlapped{};
//
//     enum class IOType {
//         UNKNOWN,
//         NEW_CONNECTION,
//         CLIENT_READ,
//         CLIENT_WRITE
//     };
//
//     IOType type;
//     socket_type socket;
//
//     SocketBuffer buffer{};
//     DWORD n_buffer_bytes = 0;
//     char lpOutputBuf[1024]{};
//     SafeQueue<AsyncSocket *> *available_buffers;
//
//     bool closed = false;
//
// public:
//     AsyncSocket(const IOType type, const socket_type socket, SafeQueue<AsyncSocket *> *available_buffers)
//         : type(type), socket(socket), available_buffers(available_buffers) {
//     }
//
//     void async_close() {
//         closed = true;
//     }
//
//     [[nodiscard]] size_t async_write(const SocketBuffer &buffer) const {
//         AsyncSocket *socket_as_buffer;
//         if (available_buffers->empty()) {
//             socket_as_buffer = new AsyncSocket(IOType::CLIENT_WRITE, -1, available_buffers);
//         } else {
//             socket_as_buffer = available_buffers->front();
//             available_buffers->pop();
//         }
//
//         const auto has_sent = buffer.p_current - buffer.buffer;
//         const DWORD size = buffer.size - has_sent;
//         socket_as_buffer->buffer.wsaBuf.len = size;
//         DWORD bytes_sent = 0;
//
//         auto ret = WSASend(socket, &socket_as_buffer->buffer.wsaBuf, 1, &bytes_sent, 0, &socket_as_buffer->overlapped, nullptr);
//         if (ret != NOERROR) {
//             auto err = GetLastError();
//             if (err != WSA_IO_PENDING) {
//                 exit(-1);
//             }
//         }
//         return bytes_sent;
//     }
//
//     void reset() {
//         closed = false;
//         socket = INVALID_SOCKET;
//         type = IOType::UNKNOWN;
//         buffer.wsaBuf.len = SocketBuffer::MAX_SIZE;
//         memset(&overlapped, 0, sizeof(OVERLAPPED));
//     }
// };

struct WSAHelper {
    OVERLAPPED overlapped{};
    char lpOutputBuf[SocketBuffer::MAX_SIZE]{};
    SocketBuffer buffer{};
};

#endif

struct AsyncSocket {
public:
#ifdef WINDOWS
    WSAHelper helper{};
    SocketBuffer buffer{};
    DWORD nBytes{};
#endif

    enum class IOType {
        UNKNOWN,
        NEW_CONNECTION,
        CLIENT_READ,
        CLIENT_WRITE
    };

private:
    IOType m_type = IOType::UNKNOWN;
    socket_type m_socket;

    bool m_is_closed = false;

public:
    std::queue<std::shared_ptr<SocketBuffer> > data_buffers{};

    AsyncSocket(const IOType type, const socket_type socket)
        : m_type(type), m_socket(socket) {
    }
#ifdef LINUX
    [[nodiscard]] ssize_t async_write(const SocketBuffer &buffer) const {
        const auto has_sent = buffer.p_current - buffer.buffer;
        return write(m_socket, buffer.p_current, buffer.size - has_sent);
    }

    void reset() {
        m_is_closed = false;
    }
#endif
#ifdef WINDOWS
    void reset(const IOType type, const socket_type socket) {
        m_type = type;
        m_socket = socket;
        m_is_closed = false;
        buffer.wsaBuf.len = SocketBuffer::MAX_SIZE;
        memset(&helper.overlapped, 0, sizeof(OVERLAPPED));
    }

    [[nodiscard]] ssize_t async_write(const SocketBuffer &buffer) const {
        const auto has_sent = buffer.p_current - buffer.buffer;
        return send(m_socket, buffer.p_current, (ssize_t) buffer.size - has_sent, 0);
    }
#endif
    void async_send(const std::vector<std::shared_ptr<SocketBuffer> > &data) {
        for (const auto &buffer: data) {
            // data_buffers.push(buffer);
            data_buffers.push(buffer);
        }
    }

    void async_close() {
        m_is_closed = true;
    }

    [[nodiscard]] bool is_closed() const {
        return m_is_closed;
    }

    [[nodiscard]] socket_type get_socket() const {
        return m_socket;
    }

    [[nodiscard]] IOType get_type() const {
        return m_type;
    }
};

struct ConnectionBehavior {
    std::function<void(AsyncSocket *, SocketBuffer &)> on_received{};
    std::function<void(AsyncSocket *)> then_response{};
};

class Multiplexing {
public:
    virtual ~Multiplexing() = default;

    virtual void setup() = 0;

    virtual void set_callback(const ConnectionBehavior &behavior) = 0;

    virtual void start() = 0;

    virtual void stop() = 0;
};

#endif //MULTIPLEXING_H
