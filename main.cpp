#include "http/TcpServer.h"

int main() {
    TcpServer server("0.0.0.0");
    ConnectionBehavior behavior;
    bool flag = false;
    behavior.on_received = [&server](AsyncSocket *socket, const SocketBuffer &buffer) {
        // std::mutex mutex;
        // std::unique_lock lock(mutex);
        // std::cout << "On received" << std::endl;
        // if (buffer.size > 0) {
        //     if (flag) {
        //         exit(-1);
        //     }
        //     flag = !flag;
        // }
        for (int i = 0; i < buffer.size; i++) {
            if (buffer.buffer[i] == '@') {
                std::cout << "\n";
                socket->async_close();
                break;
            } else if (buffer.buffer[i] == '!') {
                server.close_server();
                break;
            }
            std::cout << buffer.buffer[i];
        }
        int flag = 0;
    };
    server.on_connected(behavior);
    server.start_server();
    // server.accept_connection();
    server.close_server();
    return 0;
}
