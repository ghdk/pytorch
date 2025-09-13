#ifndef SYSTEM_H
#define SYSTEM_H

// gcc/clang -std=c++20 -x c++ -Wp,-v -E -xc -dD /dev/null

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wextra-semi"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#undef NDEBUG
#undef STRIP_ERROR_MESSAGES
#include <cassert>
#include <concepts>
#include <atomic>
#include <algorithm>
#include <random>
#include <new>
#include <filesystem>
#include <torch/extension.h>
#include <pybind11/functional.h>

#ifndef __cpp_lib_span
#include <gsl/span>
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

namespace extensions
{
template<typename ...Args>
bool assertlog(const Args& ...args)
{
    ((std::cerr << args << ' '), ...) << ": ";
    return false;
}
#define assertm(expr, ...) assert((expr) || extensions::assertlog(__VA_ARGS__))

    template <typename T> using ptr_t = std::shared_ptr<T>;
    template <typename T> using ptr_w = std::weak_ptr<T>;
    template <typename T> using ptr_u = std::unique_ptr<T>;

    static std::string demangle(std::string const& name)
    {
        int status = 0;
        std::unique_ptr<char, decltype(::free)*> ret{abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status), ::free};
        return {*ret};
    }

#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_constructive_interference_size;
    using std::hardware_destructive_interference_size;
#else
#pragma message "__cpp_lib_hardware_interference_size not defined, falling back to cache size of 64 bytes."
    constexpr std::size_t hardware_constructive_interference_size = 64;
    constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

    constexpr std::size_t hardware_destructive_interference_size_double = hardware_destructive_interference_size * 2;
    constexpr int page_size = 4096;  // bytes

}  // namespace extensions

#endif  // SYSTEM_H
