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

}}  // namespace extensions::iter

#endif  // EXTENSIONS_ITER_ITER_H
