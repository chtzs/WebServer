//
// Created by Haotian on 2024/10/2.
//

#include "MultiplexingLinux.h"


void MultiplexingLinux::exit_with_error(const std::string &message) const {
    logger->error(message);
    exit(-1);
}

void MultiplexingLinux::setup(const int socket_fd) {
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        exit_with_error("Failed to create epoll file descriptor");
    }

    epoll_event ev{
        // 设置epoll事件类型为可读事件和边缘触发
        .events = static_cast<uint32_t>(EPOLLIN | EPOLLET),
        // 将监听socket关联到epoll事件中
        .data.fd = socket_fd,
    };

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &ev) == -1) {
        // 如果添加监听socket到epoll失败，输出错误信息，关闭监听socket和epoll实例，并退出程序
        close(epoll_fd);
        close(socket_fd);
        exit_with_error("Failed to add socket to epoll file descriptor");
    }

    // Set socket_fd to non-block mode
    fcntl(socket_fd, F_SETFL, O_NONBLOCK);
}

MultiplexingLinux::MultiplexingLinux(const int socket_fd, const int max_events)
    : socket_fd(socket_fd),
      max_events(max_events),
      events(max_events) {
    logger = Logger::get_logger();
    setup(socket_fd);
}

void MultiplexingLinux::poll_sockets(std::vector<AsynSocket> &out_sockets, int &out_size) {
    const int num_events = epoll_wait(epoll_fd, events.data(), max_events, -1);

    if (num_events < 0) {
        exit_with_error("Polling failed");
    }

    for (int i = 0; i < num_events; i++) {
        out_sockets[i].epoll_fd = epoll_fd;
        out_sockets[i].fd = events[i].data.fd;
        if (events[i].data.fd == socket_fd) {
            out_sockets[i].type = AsynSocket::Type::NEW_CONNECTION;
        } else {
            out_sockets[i].type = AsynSocket::Type::CLIENT;
        }
    }
    out_size = num_events;
}

MultiplexingLinux::~MultiplexingLinux() {
    close(epoll_fd);
}
