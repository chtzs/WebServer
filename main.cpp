#include "tcp/TcpServer.h"
#include <fstream>

#include "common/SafeMap.h"
#include "http/HttpRequestParser.h"
#include "http/HttpResponse.h"

SafeMap<socket_type, HttpRequestParser> request_parsers{};
SafeMap<string, std::vector<char> > file_cache{};

void on_received(AsyncSocket *socket, const SocketBuffer &buffer) {
    if (request_parsers.contains_key(socket->get_socket())) {
        request_parsers.insert(socket->get_socket(), HttpRequestParser());
    }
    auto parser = request_parsers[socket->get_socket()];

    if (parser.feed_data(buffer)) {
        // std::cout << parser.request.method << std::endl;
        // std::cout << parser.request.protocol << std::endl;
        // std::cout << parser.request.url << std::endl;

        std::string filename;
        if (parser.request.url == "/") {
            filename = "./index.html";
        } else {
            filename = "." + parser.request.url;
        }
        HttpStatus status = HttpStatus::STATUS_OK;
        std::ifstream ifs(filename, std::ios::binary);

        if (!ifs.good()) {
            ifs = std::ifstream("./404.html");
            status = HttpStatus::STATUS_NOT_FOUND;
        }
        std::vector<char> file_buffer;
        if (file_cache.contains_key(filename)) {
            file_buffer = file_cache[filename];
        } else {
            // 获取文件大小
            ifs.seekg(0, std::ios::end);
            std::streamsize size = ifs.tellg();
            ifs.seekg(0, std::ios::beg);

            // 读取文件内容到vector中
            file_buffer = std::vector<char>(size);
            if (!ifs.read(file_buffer.data(), size)) {
                ifs.close();
                return;
            }
            file_cache.insert(filename, file_buffer);
        }
        ifs.close();

        HttpResponse response{status};
        response.set_content_type_by_url(parser.request.url);

        auto socket_buffers = response.get_response(file_buffer);

        socket->async_send(*socket_buffers);
    }
}

int main() {
    TcpServer server("0.0.0.0");
    ConnectionBehavior behavior;
    behavior.on_received = on_received;
    behavior.then_response = [&server](AsyncSocket *socket) {
        socket->async_close();
        request_parsers.remove(socket->get_socket());
    };
    server.set_callback(behavior);
    server.start_server();
    server.close_server();
    return 0;
}
