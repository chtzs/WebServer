//
// Created by Haotian on 2024/10/2.
//

#ifndef MULTIPLEXING_WINDOWS_H
#define MULTIPLEXING_WINDOWS_H


#include "Multiplexing.h"

#ifdef WINDOWS
#include <winsock2.h>
#include <condition_variable>
#include <thread>
#include "../../log/Logger.h"

class MultiplexingWindows final : public Multiplexing {
    Logger *m_logger;

    std::vector<std::thread> m_working_thread;
    std::mutex m_mutex{};
    std::condition_variable m_condition{};
    SafeQueue<AsyncSocket *> m_available_buffers{};
    bool m_is_shutdown = false;

    int number_of_threads;
    int number_of_events;

    socket_type m_socket_listen;
    HANDLE iocp_handle = nullptr;

    ConnectionBehavior m_behavior{};

    void exit_with_error(const std::string &message) const;

    void add_to_iocp(socket_type socket) const;

    void set_non_blocking(socket_type socket) const;

    // Send accept request to IOCP
    void async_post_accept(AsyncSocket *reused) const;

    void do_receive(AsyncSocket *async_socket, DWORD lpNumberOfBytesTransferred);

    void async_post_send(AsyncSocket *async_socket) const;

    void do_send(AsyncSocket *async_socket);

    // Send receive or write request to IOCP
    void async_work();

    void notify_stop();

    void wait_for_thread();

public:
    /// Bind a listen socket to IOCP
    /// @param socket_listen Socket to be bound
    /// @param number_of_threads Number of working thread
    /// @param number_of_events Maximum number of simultaneous clients on a single web server
    explicit MultiplexingWindows(
        socket_type socket_listen,
        int number_of_threads = 1,
        int number_of_events = 3000
    );

    ~MultiplexingWindows() override;

    void set_callback(const ConnectionBehavior &behavior) override;

    void setup() override;

    void start() override;

    void stop() override;
};

#endif
#endif //MULTIPLEXING_WINDOWS_H
