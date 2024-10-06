//
// Created by Haotian on 2024/10/2.
//

#ifndef MULTIPLEXING_LINUX_H
#define MULTIPLEXING_LINUX_H

#include <condition_variable>
#include <thread>

#include "Multiplexing.h"
#include "../common/Predefined.h"

#ifdef LINUX

#include "../log/Logger.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <vector>

class MultiplexingLinux final : public Multiplexing {
    std::mutex m_mutex{};
    std::condition_variable m_condition{};

    sockaddr m_address{};
    socket_len_type m_address_len;

    int m_socket_listen;
    int number_of_events;
    int number_of_threads;
    volatile bool m_is_shutdown = false;

    std::vector<std::thread> m_working_thread;
    std::vector<int> m_epoll_list;

    ConnectionBehavior m_behavior;

    Logger *m_logger = nullptr;

    void exit_with_error(const std::string &message) const;

    bool add_to_epoll(int epoll_fd, int socket_fd) const;

    bool create_epoll_fd();

    void process_loop(int epoll_fd);

    bool async_accept(int epoll_fd);

    bool async_receive(int epoll_fd, int client_fd, SocketBuffer &buffer) const;

    void notify_stop();

    void wait_for_thread();

public:
    explicit MultiplexingLinux(
        socket_type socket_listen,
        int number_of_threads = 1,
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
