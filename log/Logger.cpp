//
// Created by Haotian on 2024/9/26.
//

#include "Logger.h"

void Logger::error(const std::string &info) {
    std::cout << "Error: " << info << std::endl;
}

void Logger::info(const std::string &info) {
    std::cout << "Info: " << info << std::endl;
}

void Logger::log(const std::string &info) {
    std::cout << info;
}

Logger *Logger::get_logger() {
    static Logger *instance = nullptr;
    if (!instance) {
        instance = new Logger();
    }
    return instance;
}
//std::shared_ptr<logger> logger::get_logger() {
//
//}
