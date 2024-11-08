//
// Created by Haotian on 2024/11/5.
//

#ifndef HTTP_RANGE_H
#define HTTP_RANGE_H
#include <stringzilla.hpp>

class HttpRange {
    // 16MiB
    const int64_t MAX_RANGE_LENGTH = 16 * 1024 * 1024;

public:
    // [begin, end]
    int64_t begin = -1;
    int64_t end = -1;
    int64_t length;

    // Format: Range:(unit=first byte pos)-[last byte pos]
    explicit HttpRange(const char *range_command, const int64_t range_length = 0) : length(range_length) {
        // Range should start with "bytes="
        const sz::string_view range_command_str(range_command);
        const auto range = range_command_str.remove_prefix("bytes=");
        const auto [before, _, after] = range.partition("-");
        if (!before.strip(sz::whitespaces_set()).empty())
            begin = std::stoll(before);
        if (!after.strip(sz::whitespaces_set()).empty())
            end = std::stoll(after);

        if (begin == -1) begin = 0;
        if (end == -1) end = range_length - 1;
    }

    [[nodiscard]] std::string to_string() const {
        std::string result = "bytes " + std::to_string(begin) + "-" + std::to_string(end) + "/" +
                             std::to_string(length);
        return std::move(result);
    }
};

#endif //HTTP_RANGE_H
