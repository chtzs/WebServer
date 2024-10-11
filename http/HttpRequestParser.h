//
// Created by Haotian on 2024/10/9.
//

#ifndef HTTP_REQUEST_PARSER_H
#define HTTP_REQUEST_PARSER_H
#include "../tcp/multiplexing/Multiplexing.h"
#include <string>
#include "HttpRequest.h"
#define RETURN_FAILED() \
{\
    m_is_successful = false;\
    m_need_more = false;\
    return false;\
}

using std::string;

class HttpRequestParser {
    enum class ParseState {
        METHOD,
        URL,
        PROTOCOL,
        HEADER,
        DATA
    };

    string temp{};
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
            request.url = temp;
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
            if (!read_util_char('\r', idx, buffer, value)) {
                RETURN_FAILED()
            }
            if (idx < buffer.size && buffer[idx++] != '\r') RETURN_FAILED()
            if (idx < buffer.size && buffer[idx++] != '\n') RETURN_FAILED()
            assign_header(name, value);
        }
        // if (idx < buffer.size && buffer[idx++] != '\r') RETURN_FAILED()
        // if (idx < buffer.size && buffer[idx++] != '\n') RETURN_FAILED()
        return true;
    }

    bool parse_data(int &idx, const SocketBuffer &buffer) {
        if (!read_util_char('\r', idx, buffer, temp)) {
            RETURN_FAILED()
        }
        if (idx < buffer.size && buffer[idx++] != '\r') RETURN_FAILED()
        if (idx < buffer.size && buffer[idx++] != '\n') RETURN_FAILED()
        return true;
    }

public:
    HttpRequest request;

    bool feed_data(const SocketBuffer &buffer) {
        int idx = 0;
        if (!parse_request_line(idx, buffer)) {
            RETURN_FAILED()
        }
        if (!parse_headers(idx, buffer)) {
            RETURN_FAILED()
        }
        if (!parse_data(idx, buffer)) {
            RETURN_FAILED()
        }
        m_is_successful = true;
        m_need_more = false;
        return true;
    }

    bool need_more() const {
        return m_need_more;
    }
};


#endif //HTTP_REQUEST_PARSER_H
