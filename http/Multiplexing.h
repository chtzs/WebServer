//
// Created by Haotian on 2024/10/2.
//

#ifndef MULTIPLEXING_H
#define MULTIPLEXING_H

#include <cstdlib>
#include <vector>
#include "../common/Predefined.h"

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

#ifdef WINDOWS
struct AsynSocket {
    enum class Type {
        UNKNOWN,
        NEW_CONNECTION,
        CLIENT
    };
};
#endif

#ifdef LINUX
struct AsynSocket {
    enum class Type {
        UNKNOWN,
        NEW_CONNECTION,
        CLIENT
    };

    int epoll_fd = -1;
    int fd = -1;

    Type type = Type::UNKNOWN;

    void add(const int new_socket_fd) const {
        epoll_event ev{
            // 设置epoll事件类型为可读事件和边缘触发
            .events = static_cast<uint32_t>(EPOLLIN | EPOLLET),
            // 将监听socket关联到epoll事件中
            .data.fd = new_socket_fd,
        };

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket_fd, &ev) == -1) {
            // 如果添加监听socket到epoll失败，输出错误信息，关闭监听socket和epoll实例，并退出程序
            ::close(new_socket_fd);
            ::exit(-1);
        }

        // Set socket_fd to non-block mode
        fcntl(new_socket_fd, F_SETFL, O_NONBLOCK);
    }

    long read(char *buf, const int buf_size) const {
        return receive_bytes(fd, buf, buf_size);
    }

    void close() const {
        epoll_ctl(fd, EPOLL_CTL_DEL, fd, nullptr);
    }
};
#endif

class Multiplexing {
public:
    virtual ~Multiplexing() = default;

    virtual void poll_sockets(std::vector<AsynSocket> &out_sockets, int &out_size) = 0;
};

#endif //MULTIPLEXING_H
