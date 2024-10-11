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

    static void put_string(const shared_ptr<vector<shared_ptr<SocketBuffer> > > &response, const string &str, SocketBuffer &buffer) {
        const auto c_str = str.c_str();
        auto str_begin = c_str;
        // auto str_end = str.end();
        long long int rest_size = str.length();
        while (rest_size > 0) {
            bool create_new = false;
            long long int write_size;
            if (rest_size + buffer.size > SocketBuffer::MAX_SIZE) {
                write_size = (long long int) SocketBuffer::MAX_SIZE - buffer.size;
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

    static bool endsWith(const std::string &str, const std::string &suffix) {
        if (suffix.length() > str.length()) { return false; }

        return (str.rfind(suffix) == (str.length() - suffix.length()));
    }

public:
    explicit HttpResponse(const HttpStatus &status) : m_status(status) {
        if (status == HttpStatus::STATUS_200) {
            m_status_line = "HTTP/1.1 200 OK";
        } else if (status == HttpStatus::STATUS_404) {
            m_status_line = "HTTP/1.1 404 Not Found";
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
        }
        insert("Content-Type", content_type);
    }

    void insert(const string &key, const string &value) {
        headers.emplace(key, value);
    }

    string &operator[](const string &key) {
        return headers[key];
    }

    shared_ptr<vector<shared_ptr<SocketBuffer> > > get_response(const string &body) const {
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
        put_string(response, std::to_string(body.length()), buffer);
        put_string(response, "\r\n", buffer);
        put_string(response, "\r\n", buffer);
        put_string(response, body, buffer);
        if (buffer.p_current != buffer.buffer) {
            buffer.size = buffer.p_current - buffer.buffer;
            response->emplace_back(std::make_shared<SocketBuffer>(buffer));
        }
        return response;
    }
};

#endif //HTTP_RESPONSE_H
