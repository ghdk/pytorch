#ifndef EXTENSIONS_ITER_ITER_H
#define EXTENSIONS_ITER_ITER_H

namespace extensions { namespace iter
{

template <typename T, typename = void> struct iterator_traits;
template <typename T>
struct iterator_traits<T, std::enable_if_t<std::is_pointer_v<T>>>
{
    using type = std::iterator_traits<T>;
};
template <typename T>
struct iterator_traits<T, std::enable_if_t<!std::is_void_v<typename T::iterator_category>>>
{
    using type = T;
};
template <typename T> using iterator_traits_t = typename iterator_traits<T>::type;
template <typename T> using range_t = typename std::conditional_t<!std::is_void_v<typename iterator_traits_t<std::decay_t<T>>::iterator_category>, std::pair<T,T>, void>;

template<typename Iterable, typename T, typename = std::enable_if_t<std::is_same_v<typename Iterable::value_type, T>, void>>
bool in(Iterable const& iterable, T const& value)
{
    return iterable.end() != std::find(iterable.begin(), iterable.end(), value);
}

}}  // namespace extensions::iter

#endif  // EXTENSIONS_ITER_ITER_H
