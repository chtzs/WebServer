//
// Created by Haotian on 2024/10/15.
//

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H
#include <fstream>
#include <utility>

#include "HttpRequest.h"
#include "HttpRequestParser.h"
#include "HttpResponse.h"
#include "../common/FileReader.h"
#include "../tcp/TcpServer.h"
#include "../common/SafeMap.h"
#include "../common/UrlHelper.h"

using HttpCallback = std::function<void(HttpRequest &, HttpResponse &)>;

class HttpServer {
    int m_port;
    TcpServer m_tcp_server;
    HttpCallback m_callback;

    SafeMap<socket_type, HttpRequestParser> m_request_parsers{};
    SafeMap<string, std::vector<char> > m_file_cache{};

    void on_received(AsyncSocket *socket, const SocketBuffer &buffer) {
        if (socket->is_read_closed() || socket->is_closed()) return;
        if (buffer.size == 0) return;

        if (m_request_parsers.contains_key(socket->get_socket())) {
            m_request_parsers.emplace(socket->get_socket(), HttpRequestParser());
        }
        auto &parser = m_request_parsers[socket->get_socket()];

        bool is_successful = false;
        if (parser.need_more()) {
            is_successful = parser.feed_data(buffer);
        }

        if (!is_successful && !parser.need_more()) {
            socket->async_close();
            parser.reset();
        }

        if (is_successful) {
            // std::cout << parser.request.method << std::endl;
            // std::cout << parser.request.protocol << std::endl;
            // std::cout << parser.request.url << std::endl;
            HttpRequest req = parser.request;
            HttpResponse resp{};
            parser.reset();
            if (m_callback) m_callback(req, resp);

            auto socket_buffers = resp.get_response();
            socket->async_send(*socket_buffers);
            socket->close_read();
        }
    }

    void default_callback(const HttpRequest &req, HttpResponse &resp) {
        std::string filename;
        if (req.url == "/") {
            filename = "./index.html";
        } else {
            filename = "." + UrlHelper::decode(req.url);
        }
        HttpStatus status = HttpStatus::STATUS_OK;
        FileReader reader(filename);
        if (!reader.good()) {
            reader = FileReader("./404.html");
            status = HttpStatus::STATUS_NOT_FOUND;
        }

        if (!m_file_cache.contains_key(filename)) {
            m_file_cache.insert(filename, reader.read_all());
        }
        std::vector<char> &content = m_file_cache[filename];

        resp.set_status(status);
        resp.set_content_type_by_url(req.url);
        resp.set_body(std::make_shared<std::vector<char> >(content));
    }

public:
    explicit HttpServer(const int port)
        : m_port(port),
          m_tcp_server("0.0.0.0", port) {
        ConnectionBehavior behavior{};
        behavior.on_received = [this](AsyncSocket *socket, const SocketBuffer &buffer) {
            on_received(socket, buffer);
        };
        behavior.then_respond = [this](AsyncSocket *socket) {
            socket->async_close();
        };
        m_tcp_server.set_callback(behavior);
        reset_callback();
    }

    void set_callback(HttpCallback callback) {
        m_callback = std::move(callback);
    }

    void reset_callback() {
        m_callback = [this](const HttpRequest &req, HttpResponse &resp) {
            default_callback(req, resp);
        };
    }

    void start_server() {
        m_tcp_server.start_server();
    }

    void stop_server() {
        m_tcp_server.close_server();
    }
};

#endif //HTTP_SERVER_H
