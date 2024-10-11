#include "tcp/TcpServer.h"
#include <fstream>

#include "http/HttpRequestParser.h"
#include "http/HttpResponse.h"

int main() {
    TcpServer server("0.0.0.0");
    ConnectionBehavior behavior;
    // std::ifstream ifs("./index.html");
    // std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    // ifs.close();
    behavior.on_received = [&server](AsyncSocket *socket, const SocketBuffer &buffer) {
        HttpRequestParser parser;
        if (parser.feed_data(buffer)) {
            std::cout << parser.request.method << std::endl;
            std::cout << parser.request.protocol << std::endl;
            std::cout << parser.request.url << std::endl;

            std::string filename;
            if (parser.request.url == "/") {
                filename = "./index.html";
            } else {
                filename = "." + parser.request.url;
            }
            HttpStatus status = HttpStatus::STATUS_200;
            std::ifstream ifs(filename);
            if (!ifs.good()) {
                ifs = std::ifstream("./404.html");
                status = HttpStatus::STATUS_404;
            }

            HttpResponse response{status};
            response.set_content_type_by_url(parser.request.url);
            std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
            auto socket_buffers = response.get_response(content);
            ifs.close();

            socket->async_send(*socket_buffers);
            // socket->async_close();
        }
    };
    server.set_callback(behavior);
    server.start_server();
    server.close_server();
    return 0;
}
