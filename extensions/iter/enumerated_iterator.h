#ifndef EXTENSIONS_ITER_ENUMERATED_ITERATOR_H
#define EXTENSIONS_ITER_ENUMERATED_ITERATOR_H

namespace extensions { namespace iter
{

template<typename Iterator>
class EnumeratedIterator
{
public:  // typedefs
    using iterator_t = Iterator;
public:  // iterator
    using iterator_category = std::input_iterator_tag;
    using value_type = std::pair<size_t, typename extensions::iter::iterator_traits_t<iterator_t>::value_type>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
public:  // methods
    bool operator==(EnumeratedIterator const& other) const
    {
        return range_ == other.range_;
    }

    bool operator!=(EnumeratedIterator const& other) const
    {
        return range_ != other.range_;
    }

    value_type operator*()
    {
        return {index_, *std::get<0>(range_)};
    }

    EnumeratedIterator const& operator++()
    {
        if(std::get<0>(range_) != std::get<1>(range_))
            ++std::get<0>(range_), ++index_;
        return *this;
    }
public:  // copy/move semantics
    explicit EnumeratedIterator(iterator_t&& begin, iterator_t&& end)
    : range_{std::move(begin), std::move(end)}
    , index_{0}
    {}
    EnumeratedIterator(EnumeratedIterator const& other) = default;
    EnumeratedIterator(EnumeratedIterator&& other) = default;
    EnumeratedIterator& operator=(EnumeratedIterator const& other) = delete;
    EnumeratedIterator& operator=(EnumeratedIterator&& other) = delete;
private:  // members
    range_t<iterator_t> range_;
    size_t index_;
};

template<typename Enumerable>
class EnumeratedEnumerable
{
public:  // typedefs
    using enumerable_wrapped_t = Enumerable;
    using iterator_wrapped_t = typename std::decay<decltype(std::declval<enumerable_wrapped_t>().begin())>::type;
    using iterator_t = EnumeratedIterator<iterator_wrapped_t>;
public:  // methods
    iterator_t begin() const
    {
        return iterator_t{std::move(const_cast<enumerable_wrapped_t&>(enumerable_).begin()),
                          std::move(const_cast<enumerable_wrapped_t&>(enumerable_).end())};
    }
    iterator_t end() const
    {
        return iterator_t{std::move(const_cast<enumerable_wrapped_t&>(enumerable_).end()),
                          std::move(const_cast<enumerable_wrapped_t&>(enumerable_).end())};
    }

    iterator_t begin()
    {
        return const_cast<EnumeratedEnumerable const*>(this)->begin();
    }
    iterator_t end()
    {
        return const_cast<EnumeratedEnumerable const*>(this)->end();
    }
public:  // copy/move semantics
    explicit EnumeratedEnumerable(enumerable_wrapped_t const& e)
    : enumerable_{e}
    {}
    EnumeratedEnumerable(EnumeratedEnumerable const& other) = delete;
    EnumeratedEnumerable(EnumeratedEnumerable&& other) = delete;
    EnumeratedEnumerable& operator=(EnumeratedEnumerable const& other) = delete;
    EnumeratedEnumerable& operator=(EnumeratedEnumerable&& other) = delete;
private:  // members
    enumerable_wrapped_t const& enumerable_;

#ifdef PYDEF

    template <typename PY>
    static PY def(PY& c)
    {
        using T = typename PY::type;
        c.def(py::init<Enumerable const&, typename iterator_t::map_t&&>());
        c.def("__iter__", +[](ptr_t<T> e){ return py::make_iterator(e->begin(), e->end()); },
              py::keep_alive<0, 1>());
        return c;
    }
#endif  // PYDEF
};

template<typename E>
constexpr auto enumerate(E const& e)
{
    return EnumeratedEnumerable<E>(e);
}

}}  // namespace extensions::iter

#endif  // EXTENSIONS_ITER_ENUMERATED_ITERATOR_H
