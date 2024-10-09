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
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = socket_fd;

    // Hand over socket_fd to epoll_fd for management
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &ev) == -1) {
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
    const int client_fd = accept(m_socket_listen, &m_address, &m_address_len);
    if (client_fd < 0)
        return false;

    return add_to_epoll(epoll_fd, client_fd);
}

bool MultiplexingLinux::async_receive(const int epoll_fd, const int client_fd, SocketBuffer &buffer) const {
    // m_logger->info("Receiving data from connection.");
    AsyncSocket socket{AsyncSocket::IOType::CLIENT_READ, client_fd};
    auto close_socket = [this, client_fd, epoll_fd]() {
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr) == -1) {
            m_logger->error("Failed to receive data from client");
            return false;
        }
        close(client_fd);
        return true;
    };

    auto ret = recv(client_fd, buffer.buffer, sizeof(buffer.buffer), 0);
    // Read util ret <= 0. Epoll only notice once while receiving data, so we need to read them all from buffer.
    while (ret > 0) {
        buffer.size = ret;
        m_behavior.on_received(&socket, buffer);
        if (socket.closed) {
            return close_socket();
        }
        ret = recv(client_fd, buffer.buffer, sizeof(buffer.buffer), 0);
    }
    if (ret < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            return close_socket();
        }
    } else {
        return close_socket();
    }
    return true;
}

void MultiplexingLinux::main_accept_loop() {
    std::vector<epoll_event> events(number_of_events);
    while (!m_is_shutdown) {
        const int num_events = epoll_wait(m_main_epoll_fd, events.data(), number_of_events, -1);

        if (num_events < 0) {
            if (errno == EINTR) {
                continue;
            }
            exit_with_error("Polling failed");
        }

        // Automatically assign the accepted connection to the receiving/writing thread to process the data.
        std::vector<int> copy; {
            std::lock_guard lock(m_assign_mutex);
            copy = m_events_list;
        }
        std::vector<int> copy_indices(number_of_threads);
        for (int i = 0; i < number_of_threads; ++i) {
            copy_indices[i] = i;
        }

        std::sort(copy_indices.begin(), copy_indices.end(), [&copy](int a, int b) {
            return copy[a] < copy[b];
        });
        // [0, 1, ... assign_index] is an increased order
        int assign_index = 0;

        for (int i = 0; i < num_events; i++) {
            auto &event = events[i];
            // Shutdown
            if (event.data.fd == m_shutdown_event_fd) {
                return;
            }

            if (!(event.events & EPOLLIN)) continue;
            if (event.data.fd != m_socket_listen) {
                m_logger->error("Impossible");
                continue;
            }
            if (!async_accept(m_epoll_list[assign_index])) {
                m_logger->info("Failed to accept connection");
            }

            copy[assign_index]++;

            // Get the next suitable thread index
            if (assign_index < number_of_threads - 1
                && copy[assign_index] > copy[assign_index + 1]) {
                assign_index++;
            } else if (assign_index == number_of_threads - 1
                       && (assign_index == 0 || copy[assign_index] >= copy[assign_index - 1])) {
                assign_index = 0;
            }
        }
    }
    close(m_main_epoll_fd);
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

        m_logger->info("Count: %d\n", num_events);
        m_events_list[id] = num_events;

        for (int i = 0; i < num_events; i++) {
            auto &event = events[i];
            // Shutdown
            if (event.data.fd == m_shutdown_event_fd) {
                return;
            }

            if (!(event.events & EPOLLIN)) continue;
            if (event.data.fd == m_socket_listen) {
                m_logger->info("Impossible");
            } else {
                if (!async_receive(epoll_fd, event.data.fd, buffer)) {
                    m_logger->info("Client accidentally disconnected");
                }
            }
        }
    }
    close(epoll_fd);
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
      number_of_events(number_of_events),
      m_events_list(number_of_threads) {
    m_logger = Logger::get_logger();
    m_address_len = sizeof(sockaddr_in);
    getsockname(m_socket_listen, &m_address, &m_address_len);
}

MultiplexingLinux::~MultiplexingLinux() {
}
#endif
