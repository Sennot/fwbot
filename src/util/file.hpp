#ifndef UTIL_FILE_HPP
#define UTIL_FILE_HPP

#include <expected>
#include <fstream>
#include <iostream>

enum class FileError {
    ReadError,
};

template <typename T>
[[nodiscard]] std::expected<T, FileError> file_read(std::istream& s) {
    T value;

    s.read(reinterpret_cast<char*>(&value), sizeof(T));

    return value;
}

template <typename T>
std::expected<void, FileError> file_read_arr(std::istream& s, T* out,
                                             size_t len) {
    s.read(reinterpret_cast<char*>(out), sizeof(T) * len);

    return {};
}

#endif  // UTIL_FILE_HPP
