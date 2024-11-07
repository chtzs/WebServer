//
// Created by Haotian on 2024/11/5.
//

#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <stringzilla.hpp>
#include <filesystem>

namespace fs = std::filesystem;

class FileSystem {
    template<typename TP>
    static std::time_t to_time_t(TP tp) {
        using namespace std::chrono;
        auto sctp = time_point_cast<system_clock::duration>(tp - TP::clock::now()
                                                            + system_clock::now());
        return system_clock::to_time_t(sctp);
    }

    static std::string time_to_gmt_string(const std::time_t time) {
        // Convert time_t to GMT time
        const std::tm *gmt_time = std::gmtime(&time);
        static char buffer[30];

        // RFC 1123 Format
        std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", gmt_time);

        return {buffer};
    }

public:
    FileSystem() = delete;

    static std::string get_last_modified(const std::string &path) {
        const fs::path fs_path(path);
        const auto time = last_write_time(fs_path);
        const auto time_t = to_time_t(time);
        return time_to_gmt_string(time_t);
    }

    static bool is_directory(const std::string &path) {
        return fs::is_directory(path);
    }

    template<typename Str = std::string>
    static Str normalize_path(const Str &path) {
        Str result;
        bool is_directory = false;
        const size_t length = path.length();

        for (size_t i = 0; i < length; ++i) {
            if (path[i] == '/' || path[i] == '\\') {
                // 如果当前字符和上一个字符都是 '/'，则跳过
                if (!result.empty() && result.back() == '/') {
                    continue;
                }
            }
            result += path[i];
        }

        // 检查是否为目录路径
        if (!result.empty() && result.back() == '/') {
            is_directory = true;
        } else if (!result.empty() && path.back() == '/') {
            result += '/';
            is_directory = true;
        }

        // 移除末尾的 '/' 除非它是目录路径
        if (!is_directory && !result.empty() && result.back() == '/') {
            result.pop_back();
        }

        return std::move(result);
    }

    static bool contains_pattern(const sz::string_view &path, const sz::string_view &pattern) {
        const sz::string_view norm_path = normalize_path<sz::string>(path);
        const sz::string_view norm_pattern = normalize_path<sz::string>(pattern);
        return norm_path.starts_with(norm_pattern);
    }
};

#endif //FILE_SYSTEM_H
