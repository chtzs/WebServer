//
// Created by Haotian on 2024/10/2.
//

#ifndef MULTIPLEXING_H
#define MULTIPLEXING_H

#include <functional>
#include <cstdlib>
#include <cstring>
#include "../../common/Predefined.h"
#include "../../thread_pool/SafeQueue.h"

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
    static constexpr int MAX_SIZE = 1024;
#ifdef WINDOWS
    WSABUF wsaBuf{MAX_SIZE, buffer};
#endif
    char buffer[MAX_SIZE]{};
    size_t size = 0;
    char *p_current = buffer;

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

struct AsyncSocket {
    OVERLAPPED overlapped{};

    enum class IOType {
        UNKNOWN,
        NEW_CONNECTION,
        CLIENT_READ,
        CLIENT_WRITE
    };

    IOType type;
    socket_type socket;

    SocketBuffer buffer{};
    DWORD n_buffer_bytes = 0;
    char lpOutputBuf[1024]{};
    SafeQueue<AsyncSocket *> *available_buffers;

    bool closed = false;

public:
    AsyncSocket(const IOType type, const socket_type socket, SafeQueue<AsyncSocket *> *available_buffers)
        : type(type), socket(socket), available_buffers(available_buffers) {
    }

    void async_close() {
        closed = true;
    }

    [[nodiscard]] size_t async_write(const SocketBuffer &buffer) const {
        AsyncSocket *socket_as_buffer;
        if (available_buffers->empty()) {
            socket_as_buffer = new AsyncSocket(IOType::CLIENT_WRITE, -1, available_buffers);
        } else {
            socket_as_buffer = available_buffers->front();
            available_buffers->pop();
        }

        const auto has_sent = buffer.p_current - buffer.buffer;
        const DWORD size = buffer.size - has_sent;
        socket_as_buffer->buffer.wsaBuf.len = size;
        DWORD bytes_sent = 0;

        auto ret = WSASend(socket, &socket_as_buffer->buffer.wsaBuf, 1, &bytes_sent, 0, &socket_as_buffer->overlapped, nullptr);
        if (ret != NOERROR) {
            auto err = GetLastError();
            if (err != WSA_IO_PENDING) {
                exit(-1);
            }
        }
        return bytes_sent;
    }

    void reset() {
        closed = false;
        socket = INVALID_SOCKET;
        type = IOType::UNKNOWN;
        buffer.wsaBuf.len = SocketBuffer::MAX_SIZE;
        memset(&overlapped, 0, sizeof(OVERLAPPED));
    }
};
#endif

#ifdef LINUX
struct AsyncSocket {
    enum class IOType {
        UNKNOWN,
        NEW_CONNECTION,
        CLIENT_READ,
        CLIENT_WRITE
    };

    int epoll_fd = -1;
    int fd = -1;

    IOType type = IOType::UNKNOWN;
    socket_type socket;

    bool closed = false;

public:
    AsyncSocket(const IOType type, const socket_type socket)
        : type(type), socket(socket) {
    }

    [[nodiscard]] size_t async_write(const SocketBuffer &buffer) const {
        const auto has_sent = buffer.p_current - buffer.buffer;
        return write(socket, buffer.p_current, buffer.size - has_sent);
    }

    void async_close() {
        closed = true;
    }
};
#endif

struct ConnectionBehavior {
    std::function<void(AsyncSocket *, SocketBuffer)> on_received{};
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
