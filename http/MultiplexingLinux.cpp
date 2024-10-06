//
// Created by Haotian on 2024/10/2.
//

#include "MultiplexingLinux.h"
#ifdef LINUX

void MultiplexingLinux::exit_with_error(const std::string &message) const {
    m_logger->error(message.c_str());
    exit(-1);
}

bool MultiplexingLinux::add_to_epoll(const int epoll_fd, const int socket_fd) const {
    epoll_event ev{};
    // 水平触发模式
    ev.events = EPOLLIN | EPOLLET;
    // 将监听socket关联到epoll事件中
    ev.data.fd = socket_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &ev) == -1) {
        // 如果添加监听socket到epoll失败，输出错误信息，关闭监听socket和epoll实例，并退出程序
        close(epoll_fd);
        close(socket_fd);
        m_logger->error("Failed to add socket to epoll file descriptor");
        return false;
    }

    // Set socket_fd to non-block mode
    if (fcntl(socket_fd, F_SETFL, O_NONBLOCK)) {
        close(socket_fd);
        m_logger->error("Failed to set nonblocking mode");
        return false;
    }
    return true;
}

bool MultiplexingLinux::create_epoll_fd() {
    const auto epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        exit_with_error("Failed to create epoll file descriptor");
    }

    const bool ret = add_to_epoll(epoll_fd, m_socket_listen);
    m_epoll_list.push_back(epoll_fd);
    return ret;
}

bool MultiplexingLinux::async_accept(const int epoll_fd) {
    // m_logger->info("Accepting new connection.");
    const int client_fd = accept(m_socket_listen, &m_address, &m_address_len);
    if (client_fd < 0)
        return false;

    return add_to_epoll(epoll_fd, client_fd);
}

bool MultiplexingLinux::async_receive(const int epoll_fd, const int client_fd, SocketBuffer &buffer) const {
    // m_logger->info("Receiving data from connection.");
    AsyncSocket socket{AsyncSocket::IOType::CLIENT_READ, client_fd};
    const auto ret = recv(client_fd, buffer.buffer, sizeof(buffer.buffer), 0);
    auto close_socket = [this, client_fd, epoll_fd]() {
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr) == -1) {
            m_logger->error("Failed to receive data from client");
            return false;
        }
        close(client_fd);
        return true;
    };

    if (ret < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            return close_socket();
        }
    } else if (ret == 0) {
        return close_socket();
    } else {
        buffer.size = ret;
        m_behavior.on_received(&socket, buffer);
        if (socket.closed) {
            return close_socket();
        }
    }
    return true;
}

void MultiplexingLinux::process_loop(const int epoll_fd) {
    std::vector<epoll_event> events(number_of_events);
    SocketBuffer buffer{};
    while (!m_is_shutdown) {
        const int num_events = epoll_wait(epoll_fd, events.data(), number_of_events, -1);

        if (num_events < 0) {
            if (errno == EINTR) {
                continue;
            }
            exit_with_error("Polling failed");
        }

        for (int i = 0; i < num_events; i++) {
            auto &event = events[i];
            // readable
            if (event.events & EPOLLIN) {
                if (event.data.fd == m_socket_listen) {
                    if (!async_accept(epoll_fd)) {
                        exit_with_error("Failed to accept connection");
                    }
                } else {
                    if (!async_receive(epoll_fd, event.data.fd, buffer)) {
                        m_logger->info("Client accidentally disconnected");
                    }
                }
            }
        }
    }
}

void MultiplexingLinux::set_callback(const ConnectionBehavior &behavior) {
    m_behavior = behavior;
}

void MultiplexingLinux::notify_stop() {
    m_is_shutdown = true;
    m_condition.notify_all();
}

void MultiplexingLinux::wait_for_thread() {
    for (int i = 0; i < number_of_threads; ++i) {
        m_working_thread[i].join();
    }
}

void MultiplexingLinux::setup() {
    m_logger->info("I/O multiplexing setup.");
    for (int i = 0; i < number_of_threads; ++i) {
        if (!create_epoll_fd()) {
            exit_with_error("Setup failed");
        }
    }
}

void MultiplexingLinux::start() {
    m_logger->info("I/O multiplexing start.");
    for (const auto &fd: m_epoll_list) {
        m_working_thread.emplace_back([this, fd]() {
            process_loop(fd);
        });
    }
    std::unique_lock lock(m_mutex);
    m_condition.wait(lock, [this] {
        return m_is_shutdown;
    });
}

void MultiplexingLinux::stop() {
    m_logger->info("I/O multiplexing stop.");
    notify_stop();
    wait_for_thread();
    for (const auto fd: m_epoll_list) {
        close(fd);
    }
}

MultiplexingLinux::MultiplexingLinux(
    const int socket_listen,
    const int number_of_threads,
    const int number_of_events)
    : m_socket_listen(socket_listen),
      number_of_threads(number_of_threads),
      number_of_events(number_of_events) {
    m_logger = Logger::get_logger();
    m_address_len = sizeof(sockaddr_in);
    getsockname(m_socket_listen, &m_address, &m_address_len);
}

MultiplexingLinux::~MultiplexingLinux() {
}
#endif
