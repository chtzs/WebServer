//
// Created by Haotian on 2024/9/27.
//

#include "thread_pool/Task.h"

#include <utility>

Task::Task(std::function<void()> &&func) : func(func) {

}

void Task::invoke() const {
    this->func();
}
