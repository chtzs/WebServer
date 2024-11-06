//
// Created by Haotian on 2024/10/10.
//

#ifndef HTTP_STATUS_H
#define HTTP_STATUS_H

enum class HttpStatus {
    OK = 200,
    NOT_FOUND = 404,
    PARTIAL_CONTENT = 206,
    MOVED_PERMANENTLY = 301,
};

inline std::unordered_map<HttpStatus, std::string> http_status_to_string = {
    {HttpStatus::OK, "OK"},
    {HttpStatus::NOT_FOUND, "Not Found"},
    {HttpStatus::PARTIAL_CONTENT, "Partial Content"},
    {HttpStatus::MOVED_PERMANENTLY, "Moved Permanently"},
};

inline std::string &get_status_string(const HttpStatus status) {
    return http_status_to_string[status];
}


#endif //HTTP_STATUS_H
