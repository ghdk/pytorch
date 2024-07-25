#ifndef EXTENSIONS_ITER_FILTERED_ITERATOR_H
#define EXTENSIONS_ITER_FILTERED_ITERATOR_H

namespace extensions { namespace iter
{

template<typename Iterator>
class FilteredIterator
{
public:  // typedefs
    using iterator_t = Iterator;
    using filter_t = std::function<bool(typename extensions::iter::iterator_traits_t<iterator_t>::value_type)>;
public:  // iterator
    using iterator_category = std::input_iterator_tag;
    using value_type = typename extensions::iter::iterator_traits_t<iterator_t>::value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
public:  // methods
    bool operator==(const FilteredIterator& other) const 
    {
        return range_ == other.range_;
    }

    bool operator!=(const FilteredIterator& other) const
    {
        return range_ != other.range_;
    }

    value_type operator*()
    {
        return *std::get<0>(range_);
    }

    inline const FilteredIterator& operator++()
    {
        SeekNext();
        return *this;
    }

    inline void SeekNext() 
    {
        do
        {
            ++std::get<0>(range_);
        } while(std::get<0>(range_) != std::get<1>(range_) && !func_(*std::get<0>(range_)));
    }
public:  // copy/move semantics
    explicit FilteredIterator(Iterator&& begin, Iterator&& end, filter_t const& func)
        : range_{std::move(begin), std::move(end)}
        , func_{func}
    {
        if(std::get<0>(range_) != std::get<1>(range_) && !func_(*std::get<0>(range_)))
            SeekNext();
    }
    FilteredIterator(FilteredIterator const& other) = default;
    FilteredIterator(FilteredIterator&& other) = default;
    FilteredIterator& operator=(FilteredIterator const& other) = delete;
    FilteredIterator& operator=(FilteredIterator&& other) = delete;
private:  // members
    range_t<iterator_t> range_;
    filter_t const& func_;
};

template<typename Enumerable>
class FilteredEnumerable
{
public:  // typedefs
    using enumerable_wrapped_t = Enumerable;
    using iterator_wrapped_t = typename std::decay<decltype(std::declval<enumerable_wrapped_t>().begin())>::type;
    using iterator_t = FilteredIterator<iterator_wrapped_t>;
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
        return const_cast<FilteredEnumerable const*>(this)->begin();
    }
    iterator_t end()
    {
        return const_cast<FilteredEnumerable const*>(this)->end();
    }
public:  // copy/move semantics
    explicit FilteredEnumerable(enumerable_wrapped_t const& e, typename iterator_t::filter_t const& f)
    : enumerable_{e}, func_{f}
    {}
    FilteredEnumerable(FilteredEnumerable const& other) = delete;
    FilteredEnumerable(FilteredEnumerable&& other) = delete;
    FilteredEnumerable& operator=(FilteredEnumerable const& other) = delete;
    FilteredEnumerable& operator=(FilteredEnumerable&& other) = delete;
private:  // members
    enumerable_wrapped_t const& enumerable_;
    typename iterator_t::filter_t const& func_;

#ifdef PYDEF

    /**
     * pybind11 creates a temporary object when wrapping callbacks. The
     * enumerable needs to hold a reference to a callback instead. When
     * used with pybind11 the enumerable takes ownership of the temporary
     * object referring to the callback, but not the callback.
     */

private:
    typename iterator_t::filter_t func_wrapper_;
public:
    explicit FilteredEnumerable(Enumerable const& e, typename iterator_t::filter_t&& f)
    : enumerable_{e}
    , func_{func_wrapper_}
    , func_wrapper_{std::move(f)}
    {}

    template <typename PY>
    static PY def(PY& c)
    {
        using T = typename PY::type;
        c.def(py::init<Enumerable const&, typename iterator_t::filter_t&&>());
        c.def("__iter__", [](ptr_t<T> e){ return py::make_iterator(e->begin(), e->end()); },
              py::keep_alive<0, 1>());
        return c;
    }
#endif  // PYDEF
};

template<typename E, typename F>
constexpr auto filter(E const& e, F const& f)
{
    return FilteredEnumerable<E>(e, f);
}

}}  // namespace extensions::iter

#endif  // EXTENSIONS_ITER_FILTERED_ITERATOR_H
