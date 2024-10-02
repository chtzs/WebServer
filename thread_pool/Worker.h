//
// Created by Haotian on 2024/9/27.
//

#ifndef WEBSERVER_WORKER_H
#define WEBSERVER_WORKER_H

#include <thread>
#include <functional>
#include <iostream>
#include <condition_variable>
#include "SharedContext.h"

struct SharedContext;

class Worker {
public:
    enum class Status {
        IDLE,
        RUNNING,
        TERMINATE
    };

private:
    std::function<void()> m_on_finished;
    std::thread m_thread;
    Status m_status = Status::IDLE;
    bool m_shutdown = false;
    SharedContext *context = nullptr;

    friend void task_loop(Worker *self);

public:
    explicit Worker(SharedContext *context);

    ~Worker();

    Worker(Worker &&other) = delete;

    void shutdown();

    void join();

    void set_on_finished(const std::function<void()> &&on_finished);

    Status get_status() const;
};


#endif //WEBSERVER_WORKER_H
