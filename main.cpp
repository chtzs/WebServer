#include <iostream>
#include <chrono>
#include <sstream>
#include "http/TcpServer.h"
#include "thread_pool/ThreadPool.h"

int main() {
    // ThreadPool pool(3);
    //
    // for (int i = 0; i < 10; ++i) {
    //     pool.push_task([i]() {
    //         std::stringstream ss;
    //         ss << "Hello" << i << std::endl;
    //         std::cout << ss.str();
    //         std::this_thread::sleep_for(std::chrono::seconds(1));
    //     });
    // }
    // pool.shutdown();
    std::cout << "Hello, World!" << std::endl;
    TcpServer server("127.0.0.1");
    server.start_server();
    server.accept_connection();
    server.close_server();
    return 0;
}
