//
// Created by Haotian on 2024/9/27.
//

#include "Worker.h"

void task_loop(Worker *self) {
    while (!self->m_shutdown) {
        shared_ptr<Task> task; {
            std::cout << "Waiting..." << std::endl;
            std::unique_lock<std::mutex> lock(self->context->condition_mutex);
            self->context->condition.wait(lock, [self] {
                return self->m_shutdown || !self->context->task_queue.empty();
            });
            if (self->m_shutdown)
                break;
            // std::cout << "Get resources" << self->context->task_queue.size() << std::endl;
            task = self->context->task_queue.front();
            self->context->task_queue.pop();
        }

        self->m_status = Worker::Status::RUNNING;
        task->invoke();
        if (self->m_on_finished) self->m_on_finished();
        self->m_status = Worker::Status::IDLE;
        self->context->condition.notify_one();
    }
    self->m_status = Worker::Status::TERMINATE;
}

Worker::Worker(SharedContext *context) : context(context) {
    m_thread = std::thread(task_loop, this);
    m_on_finished = nullptr;
}

Worker::~Worker() = default;

void Worker::set_on_finished(const std::function<void()> &&on_finished) {
    m_on_finished = on_finished;
}

Worker::Status Worker::get_status() const {
    return m_status;
}

void Worker::shutdown() {
    m_shutdown = true;
}

void Worker::join() {
    if (m_thread.joinable()) {
        m_thread.join();
    }
}
