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
#include "../../common/SendQueue.h"

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

struct WSAHelper {
    OVERLAPPED overlapped{};
    char lpOutputBuf[SocketBuffer::MAX_SIZE]{};
};

#endif

struct AsyncSocket {
public:
#ifdef WINDOWS
    WSAHelper helper{};
    SocketBuffer read_buffer{};
    SocketBuffer write_buffer{};
    DWORD nBytes{};
#endif

    enum class IOType {
        CLIENT_READ,
        CLIENT_WRITE
    };

private:
    IOType m_type;
    socket_type m_socket;

    bool m_is_closed = false;
    bool m_is_read_closed = false;

public:
    SendQueue<std::shared_ptr<SocketBuffer> > send_queue{};

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
        m_is_read_closed = false;
        data_buffers.clear();
    }
#endif
#ifdef WINDOWS
    void reset(const IOType type, const socket_type socket) {
        m_type = type;
        m_socket = socket;
        m_is_closed = false;
        m_is_read_closed = false;
        send_queue.clear();
        read_buffer.wsaBuf.len = SocketBuffer::MAX_SIZE;
        write_buffer.wsaBuf.len = SocketBuffer::MAX_SIZE;
        memset(&helper.overlapped, 0, sizeof(OVERLAPPED));
    }
#endif
    void async_send(const std::vector<std::shared_ptr<SocketBuffer> > &data) {
        send_queue.add_range(data);
    }

    void async_close() {
        m_is_closed = true;
    }

    void close_read() {
        m_is_read_closed = true;
    }

    void set_type(IOType type) {
        m_type = type;
    }

    [[nodiscard]] bool is_closed() const {
        return m_is_closed;
    }

    [[nodiscard]] bool is_read_closed() const {
        return m_is_read_closed;
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
    std::function<void(AsyncSocket *)> then_respond{};
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
