#ifndef SYSTEM_H
#define SYSTEM_H
#include <torch/extension.h>
#include <pybind11/functional.h>

namespace extensions
{
#define assertm TORCH_INTERNAL_ASSERT

    template <typename T> using ptr_t = std::shared_ptr<T>;
    template <typename T> using ptr_w = std::weak_ptr<T>;
    template <typename T> using ptr_u = std::unique_ptr<T>;

    template <typename T>
    concept Iterable = !std::is_void<typename std::decay<T>::type::iterator_category>::value;
    template <Iterable T> using range_t = std::pair<T,T>;

    static std::string demangle(std::string const& name)
    {
        int status = 0;
        std::unique_ptr<char, decltype(::free)*> ret{abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status), ::free};
        return {*ret};
    }

}  // namespace extensions

#endif  // SYSTEM_H
