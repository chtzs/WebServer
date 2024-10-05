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
    std::vector<std::thread> working_thread;
    std::mutex mutex{};
    std::condition_variable condition{};

    HANDLE iocp_handle = nullptr;

    LPDWORD number_of_bytes = nullptr;

    int number_of_threads;

    socket_type socket_listen;

    ConnectionBehavior m_behavior{};

    bool is_shutdown = false;

    void async_accept(AsyncSocket *reused) const;

    void async_work() const;

public:
    explicit MultiplexingWindows(socket_type socket_listen, int number_of_threads = 20);

    void setup() override;

    void on_connected(ConnectionBehavior connection) override;

    void start() override;

    void stop() override;

    void notify_stop();

    void wait_for_thread();

    ~MultiplexingWindows() override;
};

#endif
#endif //MULTIPLEXING_WINDOWS_H
