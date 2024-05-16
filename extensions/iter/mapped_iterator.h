#ifndef EXTENSIONS_ITER_MAPPED_ITERATOR_H
#define EXTENSIONS_ITER_MAPPED_ITERATOR_H

namespace extensions { namespace iter
{

template<typename ToType, typename Iterator>
class MappedIterator
{
public:  // typedefs
    using iterator_t = Iterator;
    using map_t = std::function<ToType(typename extensions::iter::iterator_traits_t<iterator_t>::value_type)>;
public:  // iterator
    using iterator_category = std::input_iterator_tag;
    using value_type = ToType;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
public:  // methods
    bool operator==(MappedIterator const& other) const
    {
        return range_ == other.range_;
    }

    bool operator!=(MappedIterator const& other) const 
    {
        return range_ != other.range_;
    }

    value_type operator*()
    {
        return func_(*std::get<0>(range_));
    }

    MappedIterator const& operator++()
    {
        if(std::get<0>(range_) != std::get<1>(range_))
            ++std::get<0>(range_);
        return *this;
    }
public:  // copy/move semantics
    explicit MappedIterator(iterator_t&& begin, iterator_t&& end, map_t const& f)
    : range_{std::move(begin), std::move(end)}
    , func_{f}
    {}
    MappedIterator(MappedIterator const& other) = default;
    MappedIterator(MappedIterator&& other) = default;
    MappedIterator& operator=(MappedIterator const& other) = delete;
    MappedIterator& operator=(MappedIterator&& other) = delete;
private:  // members
    range_t<iterator_t> range_;
    map_t const& func_;
};

template<typename ToType, typename Enumerable>
class MappedEnumerable
{
public:  // typedefs
    using enumerable_wrapped_t = Enumerable;
    using iterator_wrapped_t = typename std::decay<decltype(std::declval<enumerable_wrapped_t>().begin())>::type;
    using iterator_t = MappedIterator<ToType, iterator_wrapped_t>;
public:  // methods
    iterator_t begin() const
    {
        return iterator_t{std::move(const_cast<enumerable_wrapped_t&>(enumerable_).begin()),
                          std::move(const_cast<enumerable_wrapped_t&>(enumerable_).end()),
                          func_};
    }
    iterator_t end() const
    {
        return iterator_t{std::move(const_cast<enumerable_wrapped_t&>(enumerable_).end()),
                          std::move(const_cast<enumerable_wrapped_t&>(enumerable_).end()),
                          func_};
    }

    iterator_t begin()
    {
        return const_cast<MappedEnumerable const*>(this)->begin();
    }
    iterator_t end()
    {
        return const_cast<MappedEnumerable const*>(this)->end();
    }
public:  // copy/move semantics
    explicit MappedEnumerable(enumerable_wrapped_t const& e, typename iterator_t::map_t const& f)
    : enumerable_{e}, func_{f}
    {}
    MappedEnumerable(MappedEnumerable const& other) = delete;
    MappedEnumerable(MappedEnumerable&& other) = delete;
    MappedEnumerable& operator=(MappedEnumerable const& other) = delete;
    MappedEnumerable& operator=(MappedEnumerable&& other) = delete;
private:  // members
    enumerable_wrapped_t const& enumerable_;
    typename iterator_t::map_t const& func_;

#ifdef PYDEF

    /**
     * pybind11 creates a temporary object when wrapping callbacks. The
     * enumerable needs to hold a reference to a callback instead. When
     * used with pybind11 the enumerable takes ownership of the temporary
     * object referring to the callback, but not the callback.
     */

private:
    typename iterator_t::map_t func_wrapper_;
public:
    explicit MappedEnumerable(Enumerable const& e, typename iterator_t::map_t&& f)
    : enumerable_{e}
    , func_{func_wrapper_}
    , func_wrapper_{std::move(f)}
    {}

    template <typename PY>
    static PY def(PY& c)
    {
        using T = typename PY::type;
        c.def(py::init<Enumerable const&, typename iterator_t::map_t&&>());
        c.def("__iter__", [](ptr_t<T> e){ return py::make_iterator(e->begin(), e->end()); },
              py::keep_alive<0, 1>());
        return c;
    }
#endif  // PYDEF
};

template<typename E, typename F>
constexpr auto map(E const& e, F const& f)
{
    using iter_t = typename std::decay_t<decltype(std::declval<E>().end())>;
    using value_t = typename extensions::iter::iterator_traits_t<iter_t>::value_type;
    using map_t = decltype(f(std::declval<value_t>()));

    return MappedEnumerable<map_t, E>(e, f);
}

}}  // namespace extensions::iter

#endif  // EXTENSIONS_ITER_MAPPED_ITERATOR_H
