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

#include <concepts>
#include <atomic>
#include <torch/extension.h>
#include <pybind11/functional.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

namespace extensions
{
#define assertm TORCH_INTERNAL_ASSERT

    template <typename T> using ptr_t = std::shared_ptr<T>;
    template <typename T> using ptr_w = std::weak_ptr<T>;
    template <typename T> using ptr_u = std::unique_ptr<T>;

    // template <typename T>
    // concept Iterable = !std::is_void<typename std::decay<T>::type::iterator_category>::value;
    // template <Iterable T> using range_t = std::pair<T,T>;

    template <typename T> using range_t = typename std::conditional<!std::is_void<typename std::decay<T>::type::iterator_category>::value, std::pair<T,T>, std::false_type>::type;

    static std::string demangle(std::string const& name)
    {
        int status = 0;
        std::unique_ptr<char, decltype(::free)*> ret{abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status), ::free};
        return {*ret};
    }

    template <typename T>
    static constexpr size_t bitsize = sizeof(T) * CHAR_BIT;

}  // namespace extensions

#endif  // SYSTEM_H
