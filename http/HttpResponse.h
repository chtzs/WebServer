//
// Created by Haotian on 2024/10/10.
//

#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include "HttpStatus.h"
#include "../tcp/multiplexing/Multiplexing.h"
using std::string;
using std::unordered_map;
using std::vector;
using std::shared_ptr;

class HttpResponse {
    unordered_map<string, string> headers{};

    HttpStatus m_status{};
    string m_status_line{};
    std::shared_ptr<string> m_body_str;
    std::shared_ptr<vector<char> > m_body_vector;

    template<typename StrLike>
    static void put_string(const shared_ptr<vector<shared_ptr<SocketBuffer> > > &response, const StrLike &str,
                           ssize_t length, SocketBuffer &buffer) {
        auto str_begin = str.data();
        // auto str_end = str.end();
        ssize_t rest_size = length;
        while (rest_size > 0) {
            bool create_new = false;
            long long int write_size;
            if (rest_size + buffer.size > SocketBuffer::MAX_SIZE) {
                write_size = SocketBuffer::MAX_SIZE - buffer.size;
                create_new = true;
            } else {
                write_size = rest_size;
            }
            memcpy(buffer.p_current, str_begin, write_size);
            buffer.size += write_size;
            buffer.p_current += write_size;
            str_begin += write_size;
            if (create_new) {
                // we need copy
                auto new_buffer = std::make_shared<SocketBuffer>(buffer);
                response->emplace_back(new_buffer);
                buffer.p_current = buffer.buffer;
                buffer.size = 0;
            }
            rest_size -= write_size;
        }
    }

    static void put_string(const shared_ptr<vector<shared_ptr<SocketBuffer> > > &response, const string &str,
                           SocketBuffer &buffer) {
        return put_string(response, str, str.length(), buffer);
    }

    static void put_string(const shared_ptr<vector<shared_ptr<SocketBuffer> > > &response, const vector<char> &str,
                           SocketBuffer &buffer) {
        return put_string(response, str, str.size(), buffer);
    }

    static bool endsWith(const std::string &str, const std::string &suffix) {
        if (suffix.length() > str.length()) { return false; }

        return (str.rfind(suffix) == (str.length() - suffix.length()));
    }

public:
    explicit HttpResponse() {
        set_status(HttpStatus::STATUS_OK);
        m_body_str = std::make_shared<string>();
        m_body_vector = std::make_shared<vector<char> >();
    }

    void set_status(const HttpStatus &status) {
        m_status = status;
        if (status == HttpStatus::STATUS_OK) {
            m_status_line = "HTTP/1.1 200 OK";
        } else if (status == HttpStatus::STATUS_NOT_FOUND) {
            m_status_line = "HTTP/1.1 404 Not Found";
        } else if (status == HttpStatus::PARTIAL_CONTENT) {
            m_status_line = "HTTP/1.1 206 Partial Content";
        }
    }

    void set_content_type_by_url(const string &url) {
        string content_type = "text/html; charset=utf-8";
        if (endsWith(url, ".png")) {
            content_type = "image/png";
        } else if (endsWith(url, ".jpg")) {
            content_type = "image/jpeg";
        } else if (endsWith(url, ".gif")) {
            content_type = "image/gif";
        } else if (endsWith(url, ".bmp")) {
            content_type = "image/bmp";
        } else if (endsWith(url, ".js")) {
            content_type = "application/javascript";
        } else {
            return;
        }
        insert("Content-Type", content_type);
    }

    void set_body(string &&body) {
        m_body_str = std::make_shared<string>(body);
    }

    void set_body(const std::shared_ptr<string> &body) {
        m_body_str = body;
    }

    void set_body(const std::shared_ptr<vector<char> > &body) {
        m_body_vector = body;
    }

    void insert(const string &key, const string &value) {
        headers.emplace(key, value);
    }

    string &operator[](const string &key) {
        return headers[key];
    }

    template<typename StrLike>
    shared_ptr<vector<shared_ptr<SocketBuffer> > > get_response(const StrLike &body, size_t length) const {
        auto response = std::make_shared<vector<shared_ptr<SocketBuffer> > >();
        SocketBuffer buffer{};
        put_string(response, m_status_line, buffer);
        put_string(response, "\r\n", buffer);

        for (const auto &pair: headers) {
            if (pair.first == "Content-Length") continue;
            put_string(response, pair.first, buffer);
            put_string(response, ": ", buffer);
            put_string(response, pair.second, buffer);
            put_string(response, "\r\n", buffer);
        }
        put_string(response, "Content-Length: ", buffer);
        put_string(response, std::to_string(length), buffer);
        put_string(response, "\r\n", buffer);
        put_string(response, "\r\n", buffer);
        put_string(response, body, buffer);
        if (buffer.p_current != buffer.buffer) {
            buffer.size = buffer.p_current - buffer.buffer;
            response->emplace_back(std::make_shared<SocketBuffer>(buffer));
        }
        return response;
    }

    shared_ptr<vector<shared_ptr<SocketBuffer> > > get_response(const vector<char> &body) const {
        return get_response(body, body.size());
    }

    shared_ptr<vector<shared_ptr<SocketBuffer> > > get_response(const string &body) const {
        return get_response(body, body.length());
    }

    shared_ptr<vector<shared_ptr<SocketBuffer> > > get_response() const {
        if (!m_body_str->empty())
            return get_response(*m_body_str);
        else {
            return get_response(*m_body_vector);
        }
    }
};

#endif //HTTP_RESPONSE_H
