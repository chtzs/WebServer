//
// Created by Haotian on 2024/9/26.
//

#ifndef WEBSERVER_LOGGER_H
#define WEBSERVER_LOGGER_H

#include <string>
#include <chrono>
#include <iostream>
#include <cstring>
#include <cstdarg>

static const char *get_time() {
    static char buf[32];
    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    const auto time_str = std::ctime(&now_time);
    strcpy(buf, time_str);
    for (int i = strlen(time_str) - 1; i >= 0; i--) {
        if (buf[i] == '\n') {
            buf[i] = '\0';
        }
    }
    return buf;
}

static void log_format(const char *type, const char *format, va_list &args) {
    const auto m_format = std::string(get_time()) + " " + type + ": " + format + "\n";

    vprintf(m_format.c_str(), args);
}

class Logger {
    Logger() = default;

    ~Logger() = default;

public:
    void error(const char *format, ...) {
        va_list args;
        va_start(args, format);
        log_format("Error", format, args);
        va_end(args);
    }

    void info(const char *format, ...) {
        va_list args;
        va_start(args, format);
        log_format("Info", format, args);
        va_end(args);
    }

    void log(const char *format) {
        std::cout << format;
    }

    static Logger *get_logger() {
        static Logger *instance = nullptr;
        if (!instance) {
            instance = new Logger();
        }
        return instance;
    }
};


#endif //WEBSERVER_LOGGER_H
