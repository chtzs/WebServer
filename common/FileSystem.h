//
// Created by Haotian on 2024/11/5.
//

#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <stringzilla.hpp>
#include <filesystem>

namespace fs = std::filesystem;

class FileSystem {
public:
    FileSystem() = delete;

    static bool is_directory(const std::string &path) {
        return fs::is_directory(path);
    }

    static std::string normalize_path(const std::string &path) {
        std::string result;
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
        const sz::string_view norm_path = normalize_path(path);
        const sz::string_view norm_pattern = normalize_path(pattern);
        return norm_path.starts_with(norm_pattern);
    }
};

#endif //FILE_SYSTEM_H
