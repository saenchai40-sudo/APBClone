#pragma once

#include <string>
#include <random>


inline std::wstring GenerateRandomString(size_t length) {
    const wchar_t charset[] = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::wstring result;
    result.reserve(length);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, sizeof(charset) / sizeof(wchar_t) - 2);
    for (size_t i = 0; i < length; ++i) {
        result += charset[distrib(gen)];
    }
    return result;
}
