//
// Created by Haotian on 2024/9/26.
//

#ifndef WEBSERVER_LOGGER_H
#define WEBSERVER_LOGGER_H

#include <string>
#include <memory>
#include <iostream>

class Logger {
private:

    Logger() = default;

    ~Logger() = default;

public:
    static Logger *get_logger();

    void log(const std::string &info);

    void error(const std::string &info);

    void info(const std::string &info);
};


#endif //WEBSERVER_LOGGER_H
