//
// Created by Haotian on 2024/10/17.
//
#ifndef FILE_READER_H
#define FILE_READER_H

#include <string>
#include <utility>
#include <codecvt>
#include <locale>
#include <fstream>
#include <vector>

#include "Predefined.h"

#ifdef WINDOWS
using string_buffer = std::wstring;
#else
using string_buffer = std::string;
#endif

class FileReader {
    std::string m_utf8_filename;
#ifdef WINDOWS
    std::wstring_convert<std::codecvt_utf8<wchar_t>> m_converter{};
#endif
    std::ifstream m_file;

public:
    void init_file_stream() {
        string_buffer buffer;
#ifdef WINDOWS
        buffer = m_converter.from_bytes(m_utf8_filename);
#else
        buffer = m_utf8_filename;
#endif
        m_file = std::ifstream(buffer.c_str(), std::ios::binary);
    }

    explicit FileReader(std::string &&utf8_filename): m_utf8_filename(std::move(utf8_filename)) {
        init_file_stream();
    }

    explicit FileReader(const std::string &utf8_filename) {
        m_utf8_filename = utf8_filename;
        init_file_stream();
    }

    FileReader(const FileReader &other)
        : m_utf8_filename(other.m_utf8_filename) {
        init_file_stream();
    }

    FileReader & operator=(const FileReader &other) {
        if (this == &other)
            return *this;
        m_utf8_filename = other.m_utf8_filename;
        init_file_stream();
        return *this;
    }

    ~FileReader() {
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    std::vector<char> read_all() {
        if (!m_file.is_open()) {
            return std::move(std::vector<char>());
        }
        m_file.seekg(0, std::ios::end);
        const std::streamsize size = m_file.tellg();
        m_file.seekg(0, std::ios::beg);

        std::vector<char> file_buffer(size);
        m_file.read(file_buffer.data(), size);
        return std::move(file_buffer);
    }

    std::string read_all_string() {
        if (!m_file.is_open()) {
            return std::move(std::string());
        }
        m_file.seekg(0, std::ios::end);
        const std::streamsize size = m_file.tellg();
        m_file.seekg(0, std::ios::beg);

        std::string file_buffer(size, 0);
        m_file.read(file_buffer.data(), size);
        return std::move(file_buffer);
    }

    bool good() const {
        return m_file.good();
    }

    void close() {
        m_file.close();
    }
};

#endif //FILE_READER_H
