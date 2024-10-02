//
// Created by Haotian on 2024/9/30.
//

#ifndef WEBSERVER_SHARED_CONTEXT_H
#define WEBSERVER_SHARED_CONTEXT_H

#include <memory>
#include <queue>
#include <thread>
#include <condition_variable>
#include <vector>
#include "Task.h"
#include "Worker.h"
#include "SafeQueue.h"

using std::shared_ptr;
using std::make_shared;
using std::thread;
using std::mutex;
using std::condition_variable;
using std::lock_guard;

class Worker;

struct SharedContext {
    SafeQueue<shared_ptr<Task>> task_queue;
    std::queue<shared_ptr<Worker>> worker_queue;
    std::vector<shared_ptr<Worker>> all_workers;
    mutex condition_mutex;
    condition_variable condition;
};


#endif //WEBSERVER_SHARED_CONTEXT_H
