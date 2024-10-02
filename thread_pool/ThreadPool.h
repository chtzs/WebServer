//
// Created by Haotian on 2024/9/27.
//

#ifndef WEBSERVER_THREAD_POOL_H
#define WEBSERVER_THREAD_POOL_H

#include <queue>
#include <memory>
#include <thread>
#include "Task.h"
#include "Worker.h"

class ThreadPool {
private:
    int num_workers;
    SharedContext context;
    shared_ptr<thread> wake_up = nullptr;

public:
    explicit ThreadPool(int num_workers);

    ~ThreadPool();

    void push_task(std::function<void()> &&todo);

    void shutdown();

    void notify_all() {
        context.condition.notify_all();
    }
};


#endif //WEBSERVER_THREAD_POOL_H
