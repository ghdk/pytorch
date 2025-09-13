#ifndef EXTENSIONS_ITER_GENERATED_ITERATOR_H
#define EXTENSIONS_ITER_GENERATED_ITERATOR_H

namespace extensions { namespace iter
{

    template<typename ValueType>
    class GeneratedIterator
    {
    public:  // typedefs
        using generator_t = std::function<std::optional<ValueType>(void)>;
    public:  // iterator
        using iterator_category = std::input_iterator_tag;
        using value_type = ValueType;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;
    public:  // methods
        bool operator==(const GeneratedIterator& other) const
        {
            return value_.has_value() == other.value_.has_value();
        }

        bool operator!=(const GeneratedIterator& other) const
        {
            return !(*this == other);
        }

        value_type operator*()
        {
            return value_.value();
        }

        inline const GeneratedIterator& operator++()
        {
            if(value_)
                value_ = func_();
            return *this;
        }
    public:  // copy/move semantics
        explicit GeneratedIterator(generator_t const& func, bool truth)
        : func_{func}
        {
            if(truth)
                value_ = func_();
        }
        GeneratedIterator(GeneratedIterator const& other) = default;
        GeneratedIterator(GeneratedIterator&& other) = default;
        GeneratedIterator& operator=(GeneratedIterator const& other) = delete;
        GeneratedIterator& operator=(GeneratedIterator&& other) = delete;
    private:  // members
        generator_t const& func_;
        std::optional<value_type> value_;
    };

    template<typename ValueType>
    class GeneratedEnumerable
    {
    public:  // typedefs
        using iterator_t = GeneratedIterator<ValueType>;

        iterator_t begin() const
        {
            return iterator_t{func_, true};
        }
        iterator_t end() const
        {
            return iterator_t{func_, false};
        }

        iterator_t begin()
        {
            return const_cast<GeneratedEnumerable const*>(this)->begin();
        }
        iterator_t end()
        {
            return const_cast<GeneratedEnumerable const*>(this)->end();
        }
    public:  // copy/move semantics
        explicit GeneratedEnumerable(typename iterator_t::generator_t const& f)
        : func_{f}
        {}
        GeneratedEnumerable(GeneratedEnumerable const& other) = delete;
        GeneratedEnumerable(GeneratedEnumerable&& other) = delete;
        GeneratedEnumerable& operator=(GeneratedEnumerable const& other) = delete;
        GeneratedEnumerable& operator=(GeneratedEnumerable&& other) = delete;
    private:  // members
        typename iterator_t::generator_t const& func_;

#ifdef PYDEF

        /**
         * pybind11 creates a temporary object when wrapping callbacks. The
         * enumerable needs to hold a reference to a callback instead. When
         * used with pybind11 the enumerable takes ownership of the temporary
         * object referring to the callback, but not the callback.
         */

    private:
        typename iterator_t::generator_t func_wrapper_;
    public:  // python

        explicit GeneratedEnumerable(typename iterator_t::generator_t&& f)
        : func_{func_wrapper_}
        , func_wrapper_{std::move(f)}
        {}

        template <typename PY>
        static PY def(PY& c)
        {
            using T = typename PY::type;
            c.def(py::init<typename T::iterator_t::generator_t&&>());
            c.def("__iter__", [](ptr_t<T> e){ return py::make_iterator(e->begin(), e->end()); },
                  py::keep_alive<0, 1>());
            return c;
        }
#endif  // PYDEF
    };

}}  // namespace extensions::iter

#endif  // EXTENSIONS_ITER_GENERATED_ITERATOR_H
