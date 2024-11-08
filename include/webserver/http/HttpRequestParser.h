//
// Created by Haotian on 2024/10/9.
//

#ifndef HTTP_REQUEST_PARSER_H
#define HTTP_REQUEST_PARSER_H

#include <string>
#include <sstream>
#include <stringzilla.hpp>

#include "HttpRequest.h"
#include "../common/FileSystem.h"
#include "../common/UrlHelper.h"
#include "../tcp/multiplexing/Multiplexing.h"

#define RETURN_FAILED() \
{\
    m_is_successful = false;\
    m_need_more = false;\
    return false;\
}

#define RETURN_SUCCESS() \
{\
    m_is_successful = true;\
    m_need_more = false;\
    return true;\
}

#define RETURN_NEED_MORE() \
{\
    m_is_successful = false;\
    m_need_more = true;\
    return true;\
}


class HttpRequestParser {
    using string = sz::string;

    enum class ParseState {
        METHOD,
        URL,
        PROTOCOL,
        HEADER,
        DATA,
        DONE
    };

    string temp{};
    std::stringstream data_ss{};
    size_t data_size = 0;
    ParseState state = ParseState::METHOD;

    bool m_is_successful = false;

    bool m_need_more = true;

    static bool read_util_char(const char end, int &idx, const SocketBuffer &buffer, string &out) {
        const auto size = buffer.size;
        if (idx >= size) {
            return false;
        }
        while (idx < size && buffer[idx] != end) {
            out += buffer[idx++];
        }
        return idx != size;
    }


    void assign_header(const string &name, const string &value) {
        if (name == "Refer") {
            request.refer = value;
        } else if (name == "Host") {
            request.host = value;
        } else if (name == "Location") {
            request.location = value;
        } else if (name == "Content-Type") {
            request.content_type = value;
        } else if (name == "Content-Length") {
            request.content_length = std::stoi(value);
        } else if (name == "Accept") {
            request.accept = value;
        } else if (name == "Accept-Language") {
            request.accept_language = value;
        } else if (name == "Accept-Encoding") {
            request.accept_encoding = value;
        } else if (name == "Keep-Alive") {
            request.keep_alive = value;
        } else if (name == "Cookie") {
            request.cookie = value;
        } else if (name == "Set-Cookie") {
            request.set_cookie = value;
        } else if (name == "User-Agent") {
            request.user_agent = value;
        } else {
            request.insert(name, value);
        }
    }

    bool parse_request_line(int &idx, const SocketBuffer &buffer) {
        if (state == ParseState::METHOD) {
            if (!read_util_char(' ', idx, buffer, temp)) {
                RETURN_FAILED()
            }
            request.method = temp;
            temp.clear();
            state = ParseState::URL;
            idx++;
        }
        if (state == ParseState::URL) {
            if (!read_util_char(' ', idx, buffer, temp)) {
                RETURN_FAILED()
            }
            request.url = FileSystem::normalize_path(UrlHelper::decode(temp));
            temp.clear();
            state = ParseState::PROTOCOL;
            idx++;
        }
        if (state == ParseState::PROTOCOL) {
            if (!read_util_char('\r', idx, buffer, temp)) {
                RETURN_FAILED()
            }
            request.protocol = temp;
            temp.clear();
            state = ParseState::HEADER;
        }

        if (idx < buffer.size && buffer[idx++] != '\r') RETURN_FAILED()
        if (idx < buffer.size && buffer[idx++] != '\n') RETURN_FAILED()
        if (state != ParseState::HEADER) RETURN_FAILED()
        return true;
    }

    bool parse_headers(int &idx, const SocketBuffer &buffer) {
        string name{};
        string value{};
        const auto size = buffer.size;
        while (idx < size && buffer[idx] != '\r') {
            name.clear();
            value.clear();
            if (!read_util_char(':', idx, buffer, name)) {
                RETURN_FAILED()
            }
            if (idx < size && buffer[idx++] != ':') RETURN_FAILED()
            while (idx < size && buffer[idx] == ' ') {
                idx++;
            }
            if (!read_util_char('\r', idx, buffer, value)) {
                RETURN_FAILED()
            }
            if (idx < size && buffer[idx++] != '\r') RETURN_FAILED()
            if (idx < size && buffer[idx++] != '\n') RETURN_FAILED()
            assign_header(name, value);
        }
        if (idx < size && buffer[idx++] != '\r') RETURN_FAILED()
        if (idx < size && buffer[idx++] != '\n') RETURN_FAILED()
        state = ParseState::DATA;
        return true;
    }

    bool parse_data(int &idx, const SocketBuffer &buffer) {
        if (request.method == "GET") {
            state = ParseState::DONE;
            RETURN_SUCCESS()
        }
        if (request.method == "POST") {
            const auto max_size = request.content_length;
            while (data_size < max_size && idx < buffer.size) {
                data_ss << buffer[idx++];
                data_size++;
            }
            if (data_size < max_size) {
                RETURN_NEED_MORE()
            }
            request.data = data_ss.str();
            state = ParseState::DONE;
            RETURN_SUCCESS()
        }
        RETURN_FAILED()
    }

    bool parse_parameters(const char *parameters, const size_t length) {
        std::stringstream ss{};
        int idx = 0;
        while (idx < length) {
            while (idx < length && parameters[idx] != '=') {
                ss << parameters[idx++];
            }
            if (idx >= length || parameters[idx++] != '=') {
                RETURN_FAILED()
            }
            std::string key = ss.str();
            ss.str("");
            while (idx < length && parameters[idx] != '&') {
                ss << parameters[idx++];
            }
            if (idx < length && parameters[idx] == '&') idx++;
            std::string value = ss.str();
            ss.str("");
            request.parameters.emplace(std::move(key), std::move(value));
        }
        return true;
    }

    bool parse_parameters() {
        if (request.method == "GET") {
            const string &url = request.url;
            int idx = 0;
            const auto size = url.length();
            // Find '?'
            while (idx < size && url[idx] != '?') {
                idx++;
            }

            const auto old_url = request.url;
            request.url = request.url.substr(0, idx);
            if (idx < size && old_url[idx] == '?') idx++;
            if (idx < size)
                return parse_parameters(old_url.c_str() + idx, size - idx);
        } else if (request.method == "POST") {
            if (request.content_type == "application/x-www-form-urlencoded") {
                const string &url = request.data;
                return parse_parameters(url.c_str(), url.length());
            }
        }
        return true;
    }

public:
    HttpRequest request;

    void reset() {
        temp.clear();
        data_ss.str("");
        data_size = 0;
        state = ParseState::METHOD;
        m_is_successful = false;
        m_need_more = true;
        request = HttpRequest();
    }

    bool feed_data(const SocketBuffer &buffer) {
        int idx = 0;
        if (state != ParseState::DATA && state != ParseState::DONE) {
            if (!parse_request_line(idx, buffer)) {
                RETURN_FAILED()
            }
            if (!parse_headers(idx, buffer)) {
                RETURN_FAILED()
            }
        }
        if (state == ParseState::DATA) {
            if (!parse_data(idx, buffer)) {
                RETURN_FAILED()
            }
        }
        if (state == ParseState::DONE) {
            if (!parse_parameters()) {
                RETURN_FAILED()
            }
        }

        return m_is_successful;
    }

    bool need_more() const {
        return m_need_more;
    }
};


#endif //HTTP_REQUEST_PARSER_H
