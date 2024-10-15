//
// Created by Haotian on 2024/9/30.
//

#ifndef WEBSERVER_SAFE_QUEUE_H
#define WEBSERVER_SAFE_QUEUE_H

#include <mutex>
#include <queue>

using std::mutex;
using std::lock_guard;

template<typename Type>
class SafeQueue {
    mutex queue_mtx{};
    std::queue<Type> queue{};

public:
    SafeQueue() = default;

    SafeQueue(const SafeQueue &other)
        : queue(other.queue) {
    }

    SafeQueue(SafeQueue &&other) noexcept
        : queue(std::move(other.queue)) {
    }

    SafeQueue &operator=(const SafeQueue &other) {
        if (this == &other)
            return *this;
        queue = other.queue;
        return *this;
    }

    SafeQueue &operator=(SafeQueue &&other) noexcept {
        if (this == &other)
            return *this;
        queue = std::move(other.queue);
        return *this;
    }

    void push(const Type &t) {
        lock_guard lock{queue_mtx};
        queue.push(t);
    }

    void pop() {
        lock_guard lock{queue_mtx};
        queue.pop();
    }

    void pop(Type &out) {
        lock_guard lock{queue_mtx};
        out = queue.front();
        queue.pop();
    }

    Type &front() {
        lock_guard lock{queue_mtx};
        return queue.front();
    }

    size_t size() {
        lock_guard lock{queue_mtx};
        return queue.size();
    }

    bool empty() {
        lock_guard lock{queue_mtx};
        return queue.empty();
    }

    void clear() {
        lock_guard lock{queue_mtx};
        while (!queue.empty()) {
            queue.pop();
        }
    }
};


#endif //WEBSERVER_SAFE_QUEUE_H
