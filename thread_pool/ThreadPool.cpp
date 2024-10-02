//
// Created by Haotian on 2024/9/27.
//

#include <iostream>
#include "ThreadPool.h"

ThreadPool::ThreadPool(const int num_workers)
    : num_workers(num_workers) {
    context.all_workers = std::vector<shared_ptr<Worker> >(num_workers);
    for (int i = 0; i < num_workers; ++i) {
        const auto worker = std::make_shared<Worker>(&context);
        // context.worker_queue.push(worker);
        context.all_workers[i] = worker;
    }
}

ThreadPool::~ThreadPool() = default;

void ThreadPool::push_task(std::function<void()> &&todo) {
    Task task{std::move(todo)};
    context.task_queue.push(std::make_shared<Task>(task));
    context.condition.notify_one();
}

void ThreadPool::shutdown() {
    thread shutdown([this] {
        // Wait util all jobs done
        {
            std::unique_lock lock(this->context.condition_mutex);
            this->context.condition.wait(lock, [this] {
                return this->context.task_queue.empty();
            });
            std::cout << "All jobs done." << std::endl;
            for (const auto &worker: context.all_workers) {
                // If its still working on task, a flag will terminate it in the next loop
                worker->shutdown();
            }
        }
        notify_all();
        for (const auto &worker: context.all_workers) {
            // Otherwise, wakeup the worker.
            worker->join();
        }
    });
    shutdown.join();
}
