#include "http/TcpServer.h"

int main() {
    TcpServer server("0.0.0.0");
    ConnectionBehavior behavior;
    behavior.on_received = [&server](AsyncSocket *socket, const SocketBuffer &buffer) {
        for (int i = 0; i < buffer.size; i++) {
            // if (buffer.buffer[i] == '@') {
            //     std::cout << "\n";
            //     socket->async_close();
            //     break;
            // } else if (buffer.buffer[i] == '!') {
            //     server.close_server();
            //     break;
            // }
            std::cout << buffer.buffer[i];
        }

        // Only read once
        if (buffer.size < 1024 && buffer.size > 0) {
            socket->async_close();
        }
    };
    server.set_callback(behavior);
    server.start_server();
    server.close_server();
    return 0;
}
