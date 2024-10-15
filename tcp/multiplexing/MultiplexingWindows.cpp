//
// Created by Haotian on 2024/10/2.
//

#include "MultiplexingWindows.h"
#ifdef WINDOWS

[[nodiscard]] static ssize_t async_write(AsyncSocket *async_socket, SocketBuffer &buffer) {
    DWORD bytes_sent = 0;
    async_socket->write_buffer = buffer;
    auto &write_buffer = async_socket->write_buffer;
    write_buffer.wsaBuf.len = write_buffer.size;
    write_buffer.wsaBuf.buf = write_buffer.buffer;
    const int ret = WSASend(async_socket->get_socket(), &write_buffer.wsaBuf, 1, &bytes_sent, 0,
                            &async_socket->helper.overlapped,
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

void MultiplexingWindows::add_to_iocp(const socket_type socket) const {
    if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket),
                               iocp_handle,
                               0,
                               0) == nullptr) {
        shutdown(socket, SD_BOTH);
        closesocket(socket);
        exit_with_error("Failed to associate socket with iocp");
    }
}

void MultiplexingWindows::set_non_blocking(const socket_type socket) const {
    unsigned long ul = 1;
    // Set socket_listen to non-block mode, but I'm not sure that this is needed.
    if (SOCKET_ERROR == ioctlsocket(socket, FIONBIO, &ul)) {
        shutdown(socket, SD_BOTH);
        closesocket(socket);
        exit_with_error("Failed to set socket to non-blocking mode");
    }
}

void MultiplexingWindows::setup() {
    m_logger->info("IOCP Setup");
    set_non_blocking(m_socket_listen);

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
    add_to_iocp(m_socket_listen);
}

void MultiplexingWindows::set_callback(const ConnectionBehavior &behavior) {
    m_behavior = behavior;
}

void MultiplexingWindows::async_post_accept(AsyncSocket *reused = nullptr) const {
    // Create a normal socket
    const SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    // m_logger->info("Accepting connection");
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
            // Just try again
            async_post_accept(async_socket);
            return;
        }
    }

    // Maybe unnecessary...
    // Set non-blocking mode
    set_non_blocking(client);

    // Associate new socket with iocp
    add_to_iocp(client);
}

void MultiplexingWindows::do_receive(AsyncSocket *async_socket, const DWORD lpNumberOfBytesTransferred) {
    DWORD dwFlags;
    // Data now is ready
    async_socket->read_buffer.size = lpNumberOfBytesTransferred;
    // Callback
    if (lpNumberOfBytesTransferred > 0)
        m_behavior.on_received(async_socket, async_socket->read_buffer);

    bool should_close = false;

    if (!async_socket->is_closed() && !async_socket->is_read_closed()) {
        const auto receive_ret = WSARecv(
            async_socket->get_socket(),
            &async_socket->read_buffer.wsaBuf,
            1,
            &async_socket->nBytes,
            &dwFlags,
            &async_socket->helper.overlapped,
            nullptr);

        auto error = WSAGetLastError();
        should_close |= receive_ret == SOCKET_ERROR && error != WSA_IO_PENDING;
        should_close |= error == 0 && async_socket->nBytes == 0;
    }
    should_close |= async_socket->is_closed();

    if (should_close) {
        async_socket->async_close();
    }
}

void MultiplexingWindows::async_post_send(AsyncSocket *async_socket) const {
    if (async_socket->send_queue.empty()) return;

    // Post a send request to iocp
    const auto first_buffer = async_socket->send_queue.get_next_data();
    async_socket->send_queue.move_next_data();
    // Get the first buffer and send to the client
    const auto ret = async_write(async_socket, *first_buffer);
    async_socket->set_type(AsyncSocket::IOType::CLIENT_WRITE);
    if (ret < 0) {
        async_socket->async_close();
        m_logger->error("Failed to send data. Error no: %d", (int)GetLastError());
    }
}

void MultiplexingWindows::do_send(AsyncSocket *async_socket) {
    auto &queue = async_socket->send_queue;
    if (!queue.empty()) {
        auto &send_buffer = queue.get_next_data();
        // Try write buffer to client
        const auto ret = async_write(async_socket, *send_buffer);
        // If failed, close socket
        if (ret < 0) {
            async_socket->async_close();
            m_logger->error("Failed to post iocp overlapped call");
        } else {
            queue.move_next_data();
        }
    } else {
        m_behavior.then_respond(async_socket);
    }
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
        // Ensure that for the same socket, there are only read or write events at the same time
        if (async_socket->is_closed()) {
            shutdown(async_socket->get_socket(), SD_BOTH);
            closesocket(async_socket->get_socket());
            async_post_accept(async_socket);
        } else if (async_socket->get_type() == AsyncSocket::IOType::CLIENT_READ
                   && async_socket->is_read_closed()
                   && async_socket->send_queue.has_uncommitted_data()) {
            async_socket->send_queue.commit();
            async_post_send(async_socket);
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
        async_post_accept();
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
