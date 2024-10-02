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
    mutex queue_mtx;
    std::queue<Type> queue;
public:
    void push(const Type &t) {
        lock_guard<mutex> lock{queue_mtx};
        queue.push(t);
    }

    void pop() {
        lock_guard<mutex> lock{queue_mtx};
        queue.pop();
    }

    void pop(Type &out) {
        lock_guard<mutex> lock{queue_mtx};
        out = queue.front();
        queue.pop();
    }

    Type &front() {
        lock_guard<mutex> lock{queue_mtx};
        return queue.front();
    }

    size_t size() {
        lock_guard<mutex> lock{queue_mtx};
        return queue.size();
    }

    bool empty() {
        lock_guard<mutex> lock{queue_mtx};
        return queue.empty();
    }
};


#endif //WEBSERVER_SAFE_QUEUE_H
