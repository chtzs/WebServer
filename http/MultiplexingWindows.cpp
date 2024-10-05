//
// Created by Haotian on 2024/10/2.
//

#include "MultiplexingWindows.h"

#include <iostream>


void MultiplexingWindows::setup() {
    unsigned long ul = 1;
    if (SOCKET_ERROR == ioctlsocket(socket_listen, FIONBIO, &ul)) {
        shutdown(socket_listen, SD_BOTH);
        closesocket(socket_listen);
    }

    iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
                                         nullptr,
                                         0,
                                         0);
    // Associate socket_listen with iocp
    CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket_listen),
                           iocp_handle,
                           0,
                           0);
}

void MultiplexingWindows::on_connected(ConnectionBehavior connection) {
    m_behavior = std::move(connection);
}

void MultiplexingWindows::async_accept(AsyncSocket *reused = nullptr) const {
    const SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    AsyncSocket *async_socket;
    if (reused) {
        async_socket = reused;
        async_socket->reset();
        async_socket->socket = client;
        async_socket->type = AsyncSocket::IOType::CLIENT_READ;
    } else {
        async_socket = new AsyncSocket(AsyncSocket::IOType::CLIENT_READ, client);
    }

    if (!AcceptEx(socket_listen,
                  client,
                  async_socket->lpOutputBuf,
                  0,
                  sizeof(sockaddr_in) + 16,
                  sizeof(sockaddr_in) + 16,
                  &async_socket->n_buffer_bytes,
                  &async_socket->overlapped)) {
        if (WSAGetLastError() != WSA_IO_PENDING) {
            closesocket(client);
            delete async_socket;
            exit(-1);
        }
    }

    unsigned long ul = 1;
    if (SOCKET_ERROR == ioctlsocket(client, FIONBIO, &ul)) {
        shutdown(client, SD_BOTH);
        closesocket(client);
    }

    if (nullptr == CreateIoCompletionPort((HANDLE) client, iocp_handle, 0, 0)) {
        shutdown(client, SD_BOTH);
        closesocket(client);
    }
}

static bool flag = false;

void MultiplexingWindows::async_work() const {
    DWORD lpNumberOfBytesTransferred;
    DWORD dwFlags;
    void *lpCompletionKey = nullptr;
    AsyncSocket *async_socket = nullptr;

    while (true) {
        const BOOL ret = GetQueuedCompletionStatus(
            iocp_handle,
            &lpNumberOfBytesTransferred,
            (PULONG_PTR) &lpCompletionKey,
            (LPOVERLAPPED *) &async_socket,
            INFINITE);
        if (!ret)
            continue;

        // 收到 PostQueuedCompletionStatus 发出的退出指令
        if (lpNumberOfBytesTransferred == -1) break;
        switch (async_socket->type) {
            case AsyncSocket::IOType::CLIENT_READ: {
                // if (flag) {
                //     exit(-1);
                // }
                async_socket->buffer.size = lpNumberOfBytesTransferred;
                m_behavior.on_received(async_socket, async_socket->buffer);

                bool should_close = false;
                if (!async_socket->closed) {
                    const auto receive_ret = WSARecv(
                        async_socket->socket,
                        &async_socket->buffer.wsaBuf,
                        1,
                        &async_socket->n_buffer_bytes,
                        &dwFlags,
                        &async_socket->overlapped,
                        nullptr);

                    auto error = WSAGetLastError();
                    bool empty_packet = receive_ret == 0
                                        && async_socket->n_buffer_bytes == 0;
                    should_close |= receive_ret == SOCKET_ERROR && error != WSA_IO_PENDING;
                    should_close |= empty_packet;
                    // 又发了一次请求
                    if (should_close) {
                        continue;
                    }
                } else {
                    should_close = true;
                }

                if (should_close) {
                    closesocket(async_socket->socket);
                    // async_accept(async_socket);
                    delete async_socket;
                    async_accept();
                }
            }
            break;
            default:



        }
    }
}

MultiplexingWindows::MultiplexingWindows(const socket_type socket_listen, const int number_of_threads)
    : number_of_threads(number_of_threads), socket_listen(socket_listen) {
}

MultiplexingWindows::~MultiplexingWindows() = default;


void MultiplexingWindows::start() {
    for (int i = 0; i < number_of_threads; ++i) {
        async_accept();
    }
    for (int i = 0; i < number_of_threads; ++i) {
        working_thread.emplace_back([this] {
            async_work();
        });
    }
    std::unique_lock lock(mutex);
    condition.wait(lock, [this] {
        return is_shutdown;
    });
}

void MultiplexingWindows::stop() {
    notify_stop();
    wait_for_thread();
    CloseHandle(iocp_handle);
}

void MultiplexingWindows::notify_stop() {
    is_shutdown = true;
    condition.notify_all();
    for (int i = 0; i < number_of_threads; ++i) {
        PostQueuedCompletionStatus(iocp_handle, -1, ULONG_PTR(nullptr), nullptr);
    }
}

void MultiplexingWindows::wait_for_thread() {
    for (int i = 0; i < number_of_threads; ++i) {
        working_thread[i].join();
    }
}
