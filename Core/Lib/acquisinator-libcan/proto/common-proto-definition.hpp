#pragma once

#include <concepts>
#include <cstdint>
#include <string>

template <typename T>
concept PrimitiveOrString =
    std::is_arithmetic_v<T> || std::same_as<T, std::string>;

template <typename F>
concept NetworkParserCallable =
    std::invocable<F, const std::string &, const std::string &, double> &&
    std::invocable<F, const std::string &, const std::string &, uint64_t> &&
    std::invocable<F, const std::string &, const std::string &, std::string>;