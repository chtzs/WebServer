//
// Created by Haotian on 2024/10/2.
//

#include "MultiplexingWindows.h"
#ifdef WINDOWS
#include <iostream>

[[nodiscard]] static ssize_t async_write(AsyncSocket *async_socket, SocketBuffer &buffer) {
    const DWORD has_sent = buffer.p_current - buffer.buffer;
    if (has_sent >= buffer.size) return 0;
    const DWORD size = buffer.size - has_sent;
    DWORD bytes_sent = 0;
    WSABUF buf{.len = size, .buf = buffer.p_current};
    const int ret = WSASend(async_socket->get_socket(), &buf, 1, &bytes_sent, 0, &async_socket->helper.overlapped,
                            nullptr);
    if (ret != NOERROR) {
        auto err = GetLastError();
        if (err != WSA_IO_PENDING) {
            return -1;
        }
        return 0;
    }
    return bytes_sent;
}

void MultiplexingWindows::exit_with_error(const std::string &message) const {
    m_logger->error(message.c_str());
    exit(-1);
}

void MultiplexingWindows::setup() {
    m_logger->info("IOCP Setup");
    unsigned long ul = 1;
    // Set socket_listen to non-block mode, but I'm not sure that this is needed.
    if (SOCKET_ERROR == ioctlsocket(m_socket_listen, FIONBIO, &ul)) {
        shutdown(m_socket_listen, SD_BOTH);
        closesocket(m_socket_listen);
        exit_with_error("Failed to setup socket listing");
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

void MultiplexingWindows::set_callback(const ConnectionBehavior &behavior) {
    m_behavior = behavior;
}

void MultiplexingWindows::async_accept(AsyncSocket *reused = nullptr) const {
    // Create a normal socket
    const SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    AsyncSocket *async_socket;
    // Reused one to save time of allocating memory.
    if (reused) {
        async_socket = reused;
        async_socket->reset(AsyncSocket::IOType::CLIENT_READ, client);
    } else {
        async_socket = new AsyncSocket(AsyncSocket::IOType::CLIENT_READ, client);
    }

    // Send accept request to IOCP. If done, the next step is to receive data from client.
    // So async_socket->type = AsyncSocket::IOType::CLIENT_READ;
    if (!AcceptEx(m_socket_listen,
                  client,
                  async_socket->helper.lpOutputBuf,
                  0,
                  sizeof(sockaddr_in) + 16,
                  sizeof(sockaddr_in) + 16,
                  &async_socket->nBytes,
                  &async_socket->helper.overlapped)) {
        // WSA_IO_PENDING means waiting util IOCP is ready to handle event.
        if (WSAGetLastError() != WSA_IO_PENDING) {
            shutdown(client, SD_BOTH);
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
        exit_with_error("Failed to accept socket");
    }

    // Associate new socket with iocp
    if (nullptr == CreateIoCompletionPort((HANDLE) client, iocp_handle, 0, 0)) {
        shutdown(client, SD_BOTH);
        closesocket(client);
        exit_with_error("Failed to accept socket");
    }
}

void MultiplexingWindows::do_receive(AsyncSocket *async_socket, const DWORD lpNumberOfBytesTransferred) {
    DWORD dwFlags;
    // Data now is ready
    async_socket->buffer.size = lpNumberOfBytesTransferred;
    // Callback
    if (lpNumberOfBytesTransferred > 0)
        m_behavior.on_received(async_socket, async_socket->buffer);

    bool should_close = false;
    // If async_socket->closed is true, it means client want to close it immediately.
    if (!async_socket->is_closed()) {
        // Otherwise, send Receive request to IOCP.
        const auto receive_ret = WSARecv(
            async_socket->get_socket(),
            &async_socket->buffer.wsaBuf,
            1,
            &async_socket->nBytes,
            &dwFlags,
            &async_socket->helper.overlapped,
            nullptr);

        auto error = WSAGetLastError();
        should_close |= receive_ret == SOCKET_ERROR && error != WSA_IO_PENDING;
        should_close |= error == 0 && async_socket->nBytes == 0;
    } else {
        should_close = true;
    }

    // async_socket->async_close();
    if (should_close) {
        shutdown(async_socket->get_socket(), SD_BOTH);
        closesocket(async_socket->get_socket());
        // Memory friendly
        // Send another accept request because we consume one.
        async_accept(async_socket);
    } else {
        if (!async_socket->data_buffers.empty())
            async_send(async_socket);
    }
}

void MultiplexingWindows::async_send(AsyncSocket *reused) const {
    if (reused->data_buffers.empty()) return;
    AsyncSocket *send_socket = new AsyncSocket(AsyncSocket::IOType::CLIENT_WRITE, reused->get_socket());
    send_socket->data_buffers = std::move(reused->data_buffers);
    const int ret = PostQueuedCompletionStatus(
        iocp_handle,
        0,
        0,
        &send_socket->helper.overlapped);
    if (!ret) {
        shutdown(send_socket->get_socket(), SD_BOTH);
        closesocket(send_socket->get_socket());
        delete send_socket;
        m_logger->error("Failed to post iocp overlapped call");
    }
}

void MultiplexingWindows::do_send(AsyncSocket *async_socket) {
    auto &queue = async_socket->data_buffers;

    ssize_t ret = -1;
    while (!queue.empty()) {
        auto &send_buffer = queue.front();
        ret = async_socket->async_write(*send_buffer);
        while (ret > 0) {
            send_buffer->p_current += ret;
            if (send_buffer->p_current - send_buffer->buffer == send_buffer->size)
                break;
            ret = async_socket->async_write(*send_buffer);
        }
        if (send_buffer->p_current - send_buffer->buffer == send_buffer->size) {
            queue.pop();
        }
    }
    // m_logger->info("%d", ret);
    delete async_socket;
}

void MultiplexingWindows::async_work() {
    DWORD lpNumberOfBytesTransferred;
    void *lpCompletionKey = nullptr;
    AsyncSocket *async_socket = nullptr;

    while (true) {
        // Try to retrieve a request from IOCP.
        // A single request is encapsulated into an AsyncSocket.
        // Block operation.
        const BOOL ret = GetQueuedCompletionStatus(
            iocp_handle,
            &lpNumberOfBytesTransferred,
            reinterpret_cast<PULONG_PTR>(&lpCompletionKey),
            reinterpret_cast<LPOVERLAPPED *>(&async_socket),
            INFINITE);
        if (!ret)
            continue;

        // m_logger->info("Closed: %d", socket_connected(async_socket->get_socket()));
        // Shutdown command
        if (lpNumberOfBytesTransferred == -1) {
            delete async_socket;
            break;
        }

        switch (async_socket->get_type()) {
            case AsyncSocket::IOType::CLIENT_READ:
                do_receive(async_socket, lpNumberOfBytesTransferred);
                break;
            case AsyncSocket::IOType::CLIENT_WRITE:
                do_send(async_socket);
                break;
            default:
                break;
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
    m_logger = Logger::get_logger();
}

MultiplexingWindows::~MultiplexingWindows() = default;


void MultiplexingWindows::start() {
    m_logger->info("IOCP Start");
    for (int i = 0; i < number_of_events; ++i) {
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
    m_logger->info("IOCP Stop");
    // Clean up.
    notify_stop();
    wait_for_thread();
    CloseHandle(iocp_handle);
    while (!m_available_buffers.empty()) {
        delete m_available_buffers.front();
        m_available_buffers.pop();
    }
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
#endif
