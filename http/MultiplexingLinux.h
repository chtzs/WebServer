//
// Created by Haotian on 2024/10/2.
//

#ifndef MULTIPLEXING_LINUX_H
#define MULTIPLEXING_LINUX_H

#include "Multiplexing.h"
#include "../common/Predefined.h"

#ifdef LINUX

#include "../log/Logger.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <vector>

class MultiplexingLinux : public Multiplexing {
private:
    int epoll_fd = -1;
    int socket_fd;
    int max_events;

    std::vector<epoll_event> events;

    Logger *logger = nullptr;

    void setup(int socket_fd);

    void exit_with_error(const std::string &message) const;

public:
    explicit MultiplexingLinux(int socket_fd, int max_events = 3000);

    void poll_sockets(std::vector<AsynSocket> &out_sockets, int &out_size) override;

    ~MultiplexingLinux() override;
};

#endif

#endif //MULTIPLEXING_LINUX_H
