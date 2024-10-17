//
// Created by Haotian on 2024/10/2.
//

#include "MultiplexingLinux.h"
#ifdef LINUX
#include "../../http/HttpResponse.h"
#include <fstream>

bool MultiplexingLinux::close_socket(const int epoll_fd, const socket_type client_fd) const {
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr) == -1) {
        m_logger->error("Failed to remove socket from epoll: %d, errno: %d", epoll_fd, errno);
        return false;
    }
    // m_logger->info("Removed socket: %d", client_fd);
    // m_socket_pool.delete_socket(client_fd);
    // shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    return true;
}

void MultiplexingLinux::exit_with_error(const std::string &message) const {
    m_logger->error(message.c_str());
    exit(-1);
}

bool MultiplexingLinux::add_to_epoll(const int epoll_fd, const int socket_fd) const {
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = socket_fd;

    // Hand over socket_fd to epoll_fd for management
    int a = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &ev);
    if (a == -1) {
        close(epoll_fd);
        m_logger->error("Failed to add socket to epoll file descriptor");
        return false;
    }

    // Set socket_fd to non-block mode
    if (fcntl(socket_fd, F_SETFL, O_NONBLOCK)) {
        close(socket_fd);
        m_logger->error("Failed to set nonblocking mode");
        return false;
    }
    // m_logger->info("Socket fd: %d", socket_fd);
    return true;
}

int MultiplexingLinux::create_epoll_fd() const {
    const auto epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        exit_with_error("Failed to create epoll file descriptor");
    }

    return epoll_fd;
}

bool MultiplexingLinux::async_accept(const int epoll_fd) {
    // m_logger->info("Accepting new connection.");
    while (true) {
        const int client_fd = accept(m_socket_listen, (sockaddr *) &m_address, &m_address_len);
        if (client_fd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                return false;
            } else {
                break;
            }
        }

        if (!add_to_epoll(epoll_fd, client_fd)) {
            return false;
        }
    }
    return true;
}

bool MultiplexingLinux::async_receive(
    AsyncSocket *socket,
    SocketBuffer &buffer) {
    auto ret = recv(socket->get_socket(), buffer.buffer, sizeof(buffer.buffer), 0);
    // Read util ret <= 0. Epoll only notice once while receiving data, so we need to read them all from buffer.
    while (ret > 0) {
        buffer.size = ret;
        m_behavior.on_received(socket, buffer);
        ret = recv(socket->get_socket(), buffer.buffer, sizeof(buffer.buffer), 0);
    }
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

bool MultiplexingLinux::async_send(AsyncSocket *socket) {
    auto &queue = socket->send_queue;
    if (queue.has_uncommitted_data()) {
        queue.submit();
    }
    while (!queue.empty()) {
        auto &send_buffer = queue.get_next_data();
        auto ret = socket->async_write(*send_buffer);
        while (ret > 0) {
            send_buffer->p_current += ret;
            if (send_buffer->p_current - send_buffer->buffer == send_buffer->size) break;
            ret = socket->async_write(*send_buffer);
        }
        if (ret == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                m_logger->error("Error while sending message. errno: %d", errno);
                return false;
            }
        }
        if (send_buffer->p_current - send_buffer->buffer == send_buffer->size) {
            queue.move_next_data();
        }
    }
    m_behavior.then_respond(socket);
    return true;
}

void MultiplexingLinux::thread_receive_write_loop(const int id) {
    std::vector<epoll_event> events(number_of_events);
    SocketBuffer buffer{};
    const int epoll_fd = m_epoll_list[id];
    while (!m_is_shutdown) {
        const int num_events = epoll_wait(epoll_fd, events.data(), number_of_events, -1);

        if (num_events < 0) {
            if (errno == EINTR) {
                continue;
            }
            exit_with_error("Polling failed");
        }

        // m_logger->info("Count: %d\n", num_events);

        for (int i = 0; i < num_events; i++) {
            auto &event = events[i];
            const int current_fd = event.data.fd;
            // Shutdown
            if (current_fd == m_shutdown_event_fd) {
                return;
            }

            if (!(event.events & EPOLLIN)) continue;
            if (current_fd == m_socket_listen) {
                m_logger->info("Impossible");
            } else {
                AsyncSocket *socket = m_socket_pool.get_or_default(current_fd);
                bool should_close = false;
                if (!async_receive(socket, buffer)) {
                    m_logger->info("Client accidentally disconnected");
                    should_close = true;
                }
                if (!async_send(socket)) {
                    m_logger->info("Client disconnected while sending data");
                    should_close = true;
                }
                should_close |= socket->is_closed();
                if (should_close) {
                    if (!close_socket(epoll_fd, current_fd)) {
                        m_logger->error("Failed to close socket");
                    }
                    socket->reset();
                }
            }
        }
    }
    close(epoll_fd);
}

void MultiplexingLinux::main_accept_loop() {
    std::vector<epoll_event> events(number_of_events);
    int assign_index = 0;
    while (!m_is_shutdown) {
        const int num_events = epoll_wait(m_main_epoll_fd, events.data(), number_of_events, -1);

        if (num_events < 0) {
            if (errno == EINTR) {
                continue;
            }
            exit_with_error("Polling failed");
        }

        for (int i = 0; i < num_events; i++) {
            auto &event = events[i];
            // Shutdown
            if (event.data.fd == m_shutdown_event_fd) {
                close(m_main_epoll_fd);
                return;
            }

            if (!(event.events & EPOLLIN)) continue;

            if (event.data.fd != m_socket_listen) {
                m_logger->error("Impossible");
                continue;
            }

            // Round-robin
            if (!async_accept(m_epoll_list[assign_index++])) {
                m_logger->info("Failed to accept connection");
            }
            if (assign_index >= number_of_threads) {
                assign_index = 0;
            }
        }
    }
    close(m_main_epoll_fd);
}

void MultiplexingLinux::set_callback(const ConnectionBehavior &behavior) {
    m_behavior = behavior;
}

void MultiplexingLinux::notify_stop() {
    m_is_shutdown = true;
}

void MultiplexingLinux::wait_for_thread() {
    for (int i = 0; i < number_of_threads; ++i) {
        m_working_thread[i].join();
    }
}

void MultiplexingLinux::setup() {
    m_logger->info("I/O multiplexing setup");
    m_main_epoll_fd = create_epoll_fd();
    if (!add_to_epoll(m_main_epoll_fd, m_socket_listen)) {
        exit_with_error("Failed to add socket to epoll file descriptor");
    }
    for (int i = 0; i < number_of_threads; ++i) {
        const auto fd = create_epoll_fd();
        m_epoll_list.emplace_back(fd);
    }

    // For shutdown epoll_wait
    socketpair(AF_UNIX, SOCK_STREAM, IPPROTO_IP, m_pipe);
    m_shutdown_event_fd = m_pipe[1];
    if (m_shutdown_event_fd == -1) {
        exit_with_error("Failed to setup quit event");
    }
    // Setup shutdown event
    epoll_event event{};
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = m_shutdown_event_fd;
    if (epoll_ctl(m_main_epoll_fd, EPOLL_CTL_ADD, m_shutdown_event_fd, &event) == -1) {
        exit_with_error("Failed to setup quit event");
    }
    for (const auto fd: m_epoll_list) {
        if (epoll_ctl(fd, EPOLL_CTL_ADD, m_shutdown_event_fd, &event) == -1) {
            exit_with_error("Failed to setup quit event");
        }
    }
}

void MultiplexingLinux::start() {
    m_logger->info("I/O multiplexing start.");
    for (int i = 0; i < number_of_threads; ++i) {
        m_working_thread.emplace_back([this, i]() {
            thread_receive_write_loop(i);
        });
    }
    main_accept_loop();

    m_logger->info("Waiting for thread...");
    wait_for_thread();
    m_logger->info("Thread closed.");
    m_logger->info("I/O multiplexing stop.");
}

void MultiplexingLinux::stop() {
    notify_stop();

    const auto signal_str = "Shutdown";
    const auto ret = write(m_pipe[0], signal_str, strlen(signal_str));
    if (ret < 0) {
        exit_with_error("Failed while shutdown the server. Abort with -1.");
    }
    m_logger->info("Shutdown signal sent. Size of written: %d", ret);
    m_logger->info("Epoll closed.");
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
    getsockname(m_socket_listen, (sockaddr *) &m_address, &m_address_len);
}

MultiplexingLinux::~MultiplexingLinux() {
}
#endif
