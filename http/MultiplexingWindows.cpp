//
// Created by Haotian on 2024/10/2.
//

#include "MultiplexingWindows.h"

#include <iostream>


void MultiplexingWindows::setup() {
    unsigned long ul = 1;
    // Set socket_listen to non-block mode, but I'm not sure that this is needed.
    if (SOCKET_ERROR == ioctlsocket(m_socket_listen, FIONBIO, &ul)) {
        shutdown(m_socket_listen, SD_BOTH);
        closesocket(m_socket_listen);
    }

    // Create IOCP handle.
    // 1. We don't need to bind any socket to handle, so we pass INVALID_HANDLE_VALUE;
    // 2. ExistingCompletionPort being nullptr means create new IOCP handle;
    // 3. CompletionKey are only used in Event, so we pass nullptr(0);
    // 4. If NumberOfConcurrentThreads is 0, the system allows as many concurrently
    // running threads as there are processors in the system.
    //    BTW, this parameter is ignored
    //    if the ExistingCompletionPort parameter is not NULL.
    iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
                                         nullptr,
                                         0,
                                         0);
    // Associate socket_listen with iocp, so that any event occurs on socket_listen
    // will be taken over by IOCP.
    CreateIoCompletionPort(reinterpret_cast<HANDLE>(m_socket_listen),
                           iocp_handle,
                           0,
                           0);
}

void MultiplexingWindows::set_callback(ConnectionBehavior behavior) {
    m_behavior = std::move(behavior);
}

void MultiplexingWindows::async_accept(AsyncSocket *reused = nullptr) const {
    // Create a normal socket
    const SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    AsyncSocket *async_socket;
    // Reused one to save time of allocating memory.
    if (reused) {
        async_socket = reused;
        async_socket->reset();
        async_socket->socket = client;
        async_socket->type = AsyncSocket::IOType::CLIENT_READ;
    } else {
        async_socket = new AsyncSocket(AsyncSocket::IOType::CLIENT_READ, client);
    }

    // Send accept request to IOCP. If done, the next step is to receive data from client.
    // So async_socket->type = AsyncSocket::IOType::CLIENT_READ;
    if (!AcceptEx(m_socket_listen,
                  client,
                  async_socket->lpOutputBuf,
                  0,
                  sizeof(sockaddr_in) + 16,
                  sizeof(sockaddr_in) + 16,
                  &async_socket->n_buffer_bytes,
                  &async_socket->overlapped)) {
        // WSA_IO_PENDING means waiting util IOCP is ready to handle event.
        if (WSAGetLastError() != WSA_IO_PENDING) {
            closesocket(client);
            delete async_socket;
            exit(-1);
        }
    }

    // Maybe unnecessary...
    unsigned long ul = 1;
    if (SOCKET_ERROR == ioctlsocket(client, FIONBIO, &ul)) {
        shutdown(client, SD_BOTH);
        closesocket(client);
    }

    // Associate new socket with iocp
    if (nullptr == CreateIoCompletionPort((HANDLE) client, iocp_handle, 0, 0)) {
        shutdown(client, SD_BOTH);
        closesocket(client);
    }
}

void MultiplexingWindows::async_work() const {
    DWORD lpNumberOfBytesTransferred;
    DWORD dwFlags;
    void *lpCompletionKey = nullptr;
    AsyncSocket *async_socket = nullptr;

    while (true) {
        // Try to retrieve a request from IOCP.
        // A single request is encapsulated into an AsyncSocket.
        // Block operation.
        const BOOL ret = GetQueuedCompletionStatus(
            iocp_handle,
            &lpNumberOfBytesTransferred,
            (PULONG_PTR) &lpCompletionKey,
            (LPOVERLAPPED *) &async_socket,
            INFINITE);
        if (!ret)
            continue;

        // Shutdown command
        if (lpNumberOfBytesTransferred == -1) break;

        switch (async_socket->type) {
            case AsyncSocket::IOType::CLIENT_READ: {
                // Data now is ready
                async_socket->buffer.size = lpNumberOfBytesTransferred;
                // Callback
                m_behavior.on_received(async_socket, async_socket->buffer);

                bool should_close = false;
                // If async_socket->closed is true, it means client want to close it immediately.
                if (!async_socket->closed) {
                    // Otherwise, send Receive request to IOCP.
                    const auto receive_ret = WSARecv(
                        async_socket->socket,
                        &async_socket->buffer.wsaBuf,
                        1,
                        &async_socket->n_buffer_bytes,
                        &dwFlags,
                        &async_socket->overlapped,
                        nullptr);

                    auto error = WSAGetLastError();
                    should_close |= receive_ret == SOCKET_ERROR && error != WSA_IO_PENDING;
                } else {
                    should_close = true;
                }

                if (should_close) {
                    closesocket(async_socket->socket);
                    // Memory friendly
                    // Send another accept request because we consume one.
                    async_accept(async_socket);
                }
            }
            break;
            default:



        }
    }
}

MultiplexingWindows::MultiplexingWindows(
    const socket_type socket_listen,
    const int number_of_threads,
    const int number_of_events)
    : number_of_threads(number_of_threads),
      number_of_events(number_of_events),
      m_socket_listen(socket_listen) {
}

MultiplexingWindows::~MultiplexingWindows() = default;


void MultiplexingWindows::start() {
    for (int i = 0; i < number_of_threads; ++i) {
        async_accept();
    }
    for (int i = 0; i < number_of_threads; ++i) {
        m_working_thread.emplace_back([this] {
            async_work();
        });
    }
    // Block the main thread util shutdown.
    std::unique_lock lock(m_mutex);
    m_condition.wait(lock, [this] {
        return m_is_shutdown;
    });
}

void MultiplexingWindows::stop() {
    // Clean up.
    notify_stop();
    wait_for_thread();
    CloseHandle(iocp_handle);
}

void MultiplexingWindows::notify_stop() {
    m_is_shutdown = true;
    m_condition.notify_all();
    for (int i = 0; i < number_of_threads; ++i) {
        PostQueuedCompletionStatus(iocp_handle, -1, ULONG_PTR(nullptr), nullptr);
    }
}

void MultiplexingWindows::wait_for_thread() {
    for (int i = 0; i < number_of_threads; ++i) {
        m_working_thread[i].join();
    }
}
