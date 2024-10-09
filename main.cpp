#include "tcp/TcpServer.h"
#include <fstream>

int main() {
    TcpServer server("0.0.0.0");
    ConnectionBehavior behavior;
    behavior.on_received = [&server](AsyncSocket *socket, const SocketBuffer &buffer) {
        for (int i = 0; i < buffer.size; i++) {
            char c = buffer.buffer[i];
            if (c == '\n') {
                // socket->async_close();
                break;
            } else if (c == '@') {
                server.close_server();
            } else if(c == '#') {
                const SocketBuffer& sent = buffer;
                auto size = socket->async_write(sent);
                std::cout << size << "\n";
            }
            std::cout << buffer.buffer[i];
        }
        std::cout << std::endl;
        // // Only read once
        // if (buffer.size < 1024 && buffer.size > 0) {
        //     socket->async_close();
        // }
    };
    server.set_callback(behavior);
    server.start_server();
    server.close_server();
    return 0;
}
