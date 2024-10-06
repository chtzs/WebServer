//
// Created by Haotian on 2024/10/2.
//

#ifndef MULTIPLEXING_WINDOWS_H
#define MULTIPLEXING_WINDOWS_H
#include <condition_variable>
#include <thread>

#include "Multiplexing.h"

#ifdef WINDOWS
#include <winsock2.h>

class MultiplexingWindows final : public Multiplexing {
    std::vector<std::thread> m_working_thread;
    std::mutex m_mutex{};
    std::condition_variable m_condition{};

    HANDLE iocp_handle = nullptr;

    int number_of_threads;

    int number_of_events;

    socket_type m_socket_listen;

    ConnectionBehavior m_behavior{};

    bool m_is_shutdown = false;

    // Send accept request to IOCP
    void async_accept(AsyncSocket *reused) const;

    // Send receive or write request to IOCP
    void async_work() const;

    void notify_stop();

    void wait_for_thread();

public:
    /// Bind a listen socket to IOCP
    /// @param socket_listen Socket to be bound
    /// @param number_of_threads Number of working thread
    /// @param number_of_events Maximum number of simultaneous clients on a single web server
    explicit MultiplexingWindows(
        socket_type socket_listen,
        int number_of_threads = 20,
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
