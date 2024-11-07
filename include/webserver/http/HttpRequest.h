//
// Created by Haotian on 2024/10/9.
//

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <stringzilla.hpp>


struct HttpRequest {
    using string = sz::string;
    template<typename K, typename V>
    using unordered_map = std::unordered_map<K, V>;

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

    string data{};

    unordered_map<string, string> headers{};
    unordered_map<string, string> parameters{};

    void insert(const string &key, const string &value) {
        headers.emplace(key, value);
    }

    string& operator[](const string &key) {
        return headers[key];
    }
};
#endif //HTTP_REQUEST_H
