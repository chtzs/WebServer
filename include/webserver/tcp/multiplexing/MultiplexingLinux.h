//
// Created by Haotian on 2024/10/2.
//

#ifndef MULTIPLEXING_LINUX_H
#define MULTIPLEXING_LINUX_H

#include <condition_variable>
#include <thread>

#include "Multiplexing.h"
#include "SocketPool.h"
#include "../../common/Predefined.h"

#ifdef LINUX

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <vector>
#include <sys/eventfd.h>
#include "../../log/Logger.h"

class MultiplexingLinux final : public Multiplexing {
    std::mutex m_mutex{}, m_assign_mutex{};
    std::condition_variable m_condition{};

    sockaddr_in m_address{};
    socket_len_type m_address_len;
    int m_main_epoll_fd{};
    int m_shutdown_event_fd{};
    int m_pipe[2]{};

    int m_socket_listen;
    int number_of_events;
    int number_of_threads;
    volatile bool m_is_shutdown = false;

    std::vector<std::thread> m_working_thread;
    std::vector<int> m_epoll_list;

    SocketPool m_socket_pool{};

    ConnectionBehavior m_behavior;

    Logger *m_logger = nullptr;

    bool close_socket(int epoll_fd, socket_type client_fd) const;

    void exit_with_error(const std::string &message) const;

    /// Hand over socket_fd to epoll_fd and set socket_fd to non-block mode
    /// @param epoll_fd epoll file descriptor
    /// @param socket_fd socket file descriptor
    /// @return whether succeed
    [[nodiscard]] bool add_to_epoll(int epoll_fd, int socket_fd) const;

    /// Create an epoll file descriptor
    /// @return epoll file descriptor
    [[nodiscard]] int create_epoll_fd() const;

    /// The main thread listens to all connections. If a new connection is to be established, it accepts the connection and automatically assigns the accepted connection to the receiving/writing thread to process the data.
    void main_accept_loop();

    /// Only responsible for reading and writing data
    /// @param id The index of epoll_fd
    void thread_receive_write_loop(int id);

    bool async_accept(int epoll_fd);

    bool async_receive(AsyncSocket *socket, SocketBuffer &buffer);

    bool async_send(AsyncSocket *socket);

    void notify_stop();

    void wait_for_thread();

public:
    explicit MultiplexingLinux(
        socket_type socket_listen,
        int number_of_threads = 12,
        int number_of_events = 3000
    );

    ~MultiplexingLinux() override;

    void set_callback(const ConnectionBehavior &behavior) override;

    void setup() override;

    void start() override;

    void stop() override;
};

#endif

#endif //MULTIPLEXING_LINUX_H
