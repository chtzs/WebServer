//
// Created by Haotian on 2024/10/15.
//

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H
#include <utility>
#include <stringzilla.hpp>

#include "HttpRange.h"
#include "HttpRequest.h"
#include "HttpRequestParser.h"
#include "HttpResponse.h"
#include "../common/FileReader.h"
#include "../common/FileSystem.h"
#include "../tcp/TcpServer.h"
#include "../common/SafeMap.h"
#include "../common/UrlHelper.h"

inline auto NOT_FOUND_HTML = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Document</title>
    <style>
        body, html{
width: 100%; height: 100%; display: flex;
 justify-content: center;
        }
    </style>
</head>
<body>
    <h1>404 Not Found</h1>
</body>
</html>
)";

using HttpCallback = std::function<void(HttpRequest &, HttpResponse &)>;

class HttpServer {
    using string = std::string;

    // If file size is bigger than this value, then it will not been cached.
    const int MAX_CACHED_SIZE = 10 * 1024 * 1024;

    int m_port;
    TcpServer m_tcp_server;
    HttpCallback m_callback;
    bool m_enable_cache = false;

    SafeMap<socket_type, HttpRequestParser> m_request_parsers{};
    SafeMap<string, std::vector<char> > m_file_cache{};
    std::unordered_map<string, HttpCallback> m_custom_request_callbacks{};

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
            HttpRequest req = parser.request;
            HttpResponse resp{};
            parser.reset();
            if (m_callback) m_callback(req, resp);

            auto socket_buffers = resp.get_response();
            socket->async_send(*socket_buffers);
            socket->close_read();
        }
    }

    bool try_handle_custom_request(HttpRequest &req, HttpResponse &resp) {
        if (m_custom_request_callbacks.count(req.url) > 0) {
            m_custom_request_callbacks[req.url](req, resp);
            return true;
        }
        return false;
    }

    static bool try_handle_not_found(const FileReader &reader, HttpRequest &req, HttpResponse &resp) {
        if (!reader.good()) {
            resp.set_status(HttpStatus::NOT_FOUND);
            resp.insert("Content-Type", "text/html; charset=utf-8");
            resp.set_body(string(NOT_FOUND_HTML));
            return true;
        }
        return false;
    }

    static bool try_handle_range(FileReader &reader, HttpRequest &req, HttpResponse &resp) {
        if (req.headers.count("Range") > 0) {
            const HttpRange range(req.headers["Range"].c_str(), reader.size());
            const auto range_str = range.to_string();
            resp.set_status(HttpStatus::PARTIAL_CONTENT);
            resp.insert("Accept-Ranges", "bytes");
            resp.insert("Content-Range", range_str);
            resp.set_body(std::make_shared<std::vector<char> >(reader.read_range(range)));
            Logger::get_logger()->info("Range: %s", req.headers["Range"].c_str());
            return true;
        }
        return false;
    }

    void handle_cached_file(const std::string &filename, FileReader &reader, HttpRequest &req, HttpResponse &resp) {
        // Try fetch data from cache
        if (m_enable_cache && reader.size() <= MAX_CACHED_SIZE) {
            if (!m_file_cache.contains_key(filename)) {
                m_file_cache.insert(filename, reader.read_all());
            }
            std::vector<char> &content = m_file_cache[filename];
            resp.set_body(std::make_shared<std::vector<char> >(content));
        } else {
            resp.set_body(std::make_shared<std::vector<char> >(reader.read_all()));
        }

        resp.set_status(HttpStatus::OK);
        resp.set_content_type_by_url(req.url);
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

    void enable_cache() {
        m_enable_cache = true;
    }

    void disable_cache() {
        m_enable_cache = false;
    }

    void add_custom_request_callback(const string &url, HttpCallback &&callback) {
        m_custom_request_callbacks.emplace(url, std::move(callback));
    }

    void remove_custom_request_callback(const string &url) {
        m_custom_request_callbacks.erase(url);
    }

    void set_callback(HttpCallback callback) {
        m_callback = std::move(callback);
    }

    void default_callback(HttpRequest &req, HttpResponse &resp) {
        if (try_handle_custom_request(req, resp)) return;

        const std::string url = req.url;
        std::string filename = "./" + url;
        if (FileSystem::is_directory(filename)) {
            if (filename.back() != '/') {
                resp.insert("Location", url + "/");
                resp.set_status(HttpStatus::MOVED_PERMANENTLY);
                return;
            }
            filename += "/index.html";
            filename = FileSystem::normalize_path(filename);
        }

        FileReader reader(filename);
        if (try_handle_not_found(reader, req, resp)) return;

        resp.insert("Last-Modified", FileSystem::get_last_modified(filename));

        // Support for range
        // For now, no cache for partial content.
        if (try_handle_range(reader, req, resp)) return;

        handle_cached_file(filename, reader, req, resp);
    }

    void reset_callback() {
        m_callback = [this](HttpRequest &req, HttpResponse &resp) {
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
