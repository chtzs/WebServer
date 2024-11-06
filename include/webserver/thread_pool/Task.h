//
// Created by Haotian on 2024/9/27.
//

#ifndef WEBSERVER_TASK_H
#define WEBSERVER_TASK_H

#include <thread>
#include <functional>

class Task {
private:
    std::function<void()> func;
public:
    explicit Task(std::function<void()> &&func);

    void invoke() const;
};

static auto DEFAULT_TASK = Task([](){});

#endif //WEBSERVER_TASK_H
