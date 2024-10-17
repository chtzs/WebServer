//
// Created by Haotian on 2024/10/16.
//

#ifndef MULTIPLEXING_WINDOWS2_H
#define MULTIPLEXING_WINDOWS2_H
#include "Multiplexing.h"
#include "../../log/Logger.h"
#ifdef WINDOWS
[[nodiscard]] static ssize_t async_write(AsyncSocket *async_socket, SocketBuffer &buffer) {
    DWORD bytes_sent = 0;
    async_socket->write_buffer = buffer;
    auto &write_buffer = async_socket->write_buffer;
    write_buffer.wsaBuf.len = write_buffer.size;
    write_buffer.wsaBuf.buf = write_buffer.buffer;
    const int ret = WSASend(async_socket->get_socket(), &write_buffer.wsaBuf, 1, &bytes_sent, 0,
                            &async_socket->overlapped,
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

class MultiplexingWindows2 : public Multiplexing {
    Logger *m_logger{};
    HANDLE iocpHandle{};

    void ExitWithError(const std::string &message) const {
        m_logger->error(message.c_str());
        exit(-1);
    }

    void AddToIOCP(const socket_type socket) const {
        if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket),
                                   iocpHandle,
                                   0,
                                   0) == nullptr) {
            shutdown(socket, SD_BOTH);
            closesocket(socket);
            ExitWithError("Failed to associate socket with iocp");
        }
    }

    void SetNonBlocking(const socket_type socket) const {
        unsigned long ul = 1;
        // Set socket_listen to non-block mode, but I'm not sure that this is needed.
        if (SOCKET_ERROR == ioctlsocket(socket, FIONBIO, &ul)) {
            shutdown(socket, SD_BOTH);
            closesocket(socket);
            ExitWithError("Failed to set socket to non-blocking mode");
        }
    }

    bool GetAvailableSocket(AsyncSocket *&socket, DWORD &lpNumberOfBytesTransferred) const {
        void *lpCompletionKey = nullptr;
        const BOOL ret = GetQueuedCompletionStatus(
            iocpHandle,
            &lpNumberOfBytesTransferred,
            reinterpret_cast<PULONG_PTR>(&lpCompletionKey),
            reinterpret_cast<LPOVERLAPPED *>(&socket),
            INFINITE);
        return ret != FALSE;
    }

    bool PostAccept(AsyncSocket *socket) {
        // Create a normal socket
        const SOCKET client = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, nullptr, 0, WSA_FLAG_OVERLAPPED);
        if (INVALID_SOCKET == client) {
            return false;
        }
        socket->set_type(AsyncSocket::IOType::ACCEPT);
        socket->set_socket(client);

        if (!AcceptEx(m_socket_listen,
                      client,
                      socket->lpOutputBuf,
                      0,
                      sizeof(sockaddr_in) + 16,
                      sizeof(sockaddr_in) + 16,
                      &socket->nBytes,
                      &socket->overlapped)) {
            // WSA_IO_PENDING means waiting util IOCP is ready to handle event.
            if (WSAGetLastError() != WSA_IO_PENDING) {
                return false;
            }
        }
        return true;
    }

    bool PostReceive(AsyncSocket *socket) {
        if (socket->is_read_closed() || socket->is_closed()) return true;
        DWORD dwFlags = 0;
        socket->reset();
        socket->set_type(AsyncSocket::IOType::CLIENT_READ);
        const auto ret = WSARecv(
            socket->get_socket(),
            &socket->read_buffer.wsaBuf,
            1,
            &socket->nBytes,
            &dwFlags,
            &socket->overlapped,
            nullptr);
        auto err = WSAGetLastError();
        if ((SOCKET_ERROR == ret) && (WSA_IO_PENDING != WSAGetLastError())) {
            return false;
        }
        return true;
    }

    bool PostSend(AsyncSocket *socket) {
        if (socket->send_queue.empty()) return false;
        if (socket->is_closed()) {
            m_logger->error("What???");
            return false;
        }
        socket->set_type(AsyncSocket::IOType::CLIENT_WRITE);
        // Post a send request to iocp
        const auto firstBuffer = socket->send_queue.get_next_data();
        socket->send_queue.move_next_data();
        // Get the first buffer and send to the client
        const auto ret = async_write(socket, *firstBuffer);
        if (ret < 0) {
            m_logger->error(
                "Failed to send data. Error no: %d",
                (int) GetLastError()
            );
        }
        return ret >= 0;
    }

    bool DoAccept(AsyncSocket *socket) {
        socket_type client = socket->get_socket();
        auto *newSocket = new AsyncSocket(
            AsyncSocket::IOType::CLIENT_READ,
            client);

        AddToIOCP(client);
        SetNonBlocking(client);

        if (!PostReceive(newSocket)) {
            m_logger->error("Failed to post receive request");
            delete newSocket;
            return false;
        }

        socket->reset(AsyncSocket::IOType::ACCEPT, -1);
        if (!PostAccept(socket)) {
            m_logger->error("Failed to post accept request");
        }
        return true;
    }

    bool DoReceive(AsyncSocket *socket) {
        if (m_behavior.on_received)
            m_behavior.on_received(socket, socket->read_buffer);
        if (!PostReceive(socket)) {
            return false;
        }
        if (socket->is_read_closed() && socket->send_queue.has_uncommitted_data()) {
            socket->send_queue.submit();
            return PostSend(socket);
        }
        return true;
    }

    bool DoSend(AsyncSocket *socket) {
        auto &queue = socket->send_queue;
        if (!queue.empty()) {
            auto &send_buffer = queue.get_next_data();
            // Try write buffer to client
            const auto ret = async_write(socket, *send_buffer);
            // If failed, close socket
            if (ret < 0) {
                m_logger->error("Failed to post iocp overlapped call");
                return false;
            }
            queue.move_next_data();
        } else {
            m_behavior.then_respond(socket);
        }
        return true;
    }

    void ProcessLoop() {
        DWORD lpNumberOfBytesTransferred;
        AsyncSocket *socket = nullptr;
        while (true) {
            if (!GetAvailableSocket(socket, lpNumberOfBytesTransferred)) continue;
            if (lpNumberOfBytesTransferred == -1) break;

            bool success = true;
            switch (socket->get_type()) {
                case AsyncSocket::IOType::ACCEPT:
                    success = DoAccept(socket);
                    break;
                case AsyncSocket::IOType::CLIENT_READ:
                    socket->read_buffer.size = lpNumberOfBytesTransferred;
                    success = DoReceive(socket);
                    break;
                case AsyncSocket::IOType::CLIENT_WRITE:
                    success = DoSend(socket);
                    break;
                default:
                    break;
            }
            if (!success || socket->is_closed()) {
                closesocket(socket->get_socket());
                delete socket;
            }
        }
    }

private:
    socket_type m_socket_listen;
    int number_of_events = 0;
    ConnectionBehavior m_behavior{};

public:
    explicit MultiplexingWindows2(
        const socket_type socket_listen,
        int number_of_threads = 1,
        const int number_of_events = 3000)
        : m_socket_listen(socket_listen),
          number_of_events(number_of_events) {
        m_logger = Logger::get_logger();
    }

    void set_callback(const ConnectionBehavior &behavior) override {
        m_behavior = behavior;
    }

    void setup() override {
        m_logger->info("IOCP Setup");
        SetNonBlocking(m_socket_listen);

        // Create IOCP handle.
        // 1. We don't need to bind any socket to handle, so we pass INVALID_HANDLE_VALUE;
        // 2. ExistingCompletionPort being nullptr means create new IOCP handle;
        // 3. CompletionKey are only used in Event, so we pass nullptr(0);
        // 4. If NumberOfConcurrentThreads is 0, the system allows as many concurrently
        // running threads as there are processors in the system.
        //    BTW, this parameter is ignored
        //    if the ExistingCompletionPort parameter is not NULL.
        iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
                                            nullptr,
                                            0,
                                            0);
        // Associate socket_listen with iocp, so that any event occurs on socket_listen
        // will be taken over by IOCP.
        AddToIOCP(m_socket_listen);
    }

    void start() override {
        m_logger->info("IOCP Start");
        for (int i = 0; i < number_of_events; ++i) {
            PostAccept(new AsyncSocket());
        }
        ProcessLoop();
    }

    void stop() override {
    }
};
#endif
#endif //MULTIPLEXING_WINDOWS2_H
