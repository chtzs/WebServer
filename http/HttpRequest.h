//
// Created by Haotian on 2024/10/9.
//

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H
#include <string>
#include <unordered_map>
using std::string;
using std::unordered_map;

struct HttpRequest {
    string method{};
    string url{};
    string protocol{};

    string refer{};
    string host{};
    string location{};

    string content_type{};
    size_t content_length{};

    string accept{};
    string accept_language{};
    string accept_encoding{};

    string keep_alive{};

    string cookie{};
    string set_cookie{};

    string user_agent{};

    unordered_map<string, string> headers{};

    void insert(const string &key, const string &value) {
        headers.emplace(key, value);
    }

    string& operator[](const string &key) {
        return headers[key];
    }
};
#endif //HTTP_REQUEST_H
