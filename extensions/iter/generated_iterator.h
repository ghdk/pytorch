#ifndef EXTENSIONS_ITER_GENERATED_ITERATOR_H
#define EXTENSIONS_ITER_GENERATED_ITERATOR_H

namespace extensions { namespace iter
{

    template<typename ValueType>
    class GeneratedIterator
    {
    public:  // typedefs
        using generator_t = std::function<std::pair<ValueType, bool>(void)>;
    public:  // iterator
        using iterator_category = std::input_iterator_tag;
        using value_type = ValueType;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;
    public:  // methods
        bool operator==(const GeneratedIterator& other) const
        {
            return truth_ == other.truth_;
        }

        bool operator!=(const GeneratedIterator& other) const
        {
            return !(*this == other);
        }

        value_type operator*()
        {
            return value_;
        }

        inline const GeneratedIterator& operator++()
        {
            if(truth_)
                std::tie(value_, truth_) = func_();
            return *this;
        }
    public:  // copy/move semantics
        explicit GeneratedIterator(generator_t const& func, bool truth)
        : func_{func}, truth_{truth}
        {
            if(truth_)
                std::tie(value_, truth_) = func_();
        }
        GeneratedIterator(GeneratedIterator const& other) = delete;
        GeneratedIterator(GeneratedIterator&& other) = default;
        GeneratedIterator& operator=(GeneratedIterator const& other) = delete;
        GeneratedIterator& operator=(GeneratedIterator&& other) = delete;
    private:  // members
        generator_t func_;
        bool truth_;
        value_type value_;
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
            return static_cast<GeneratedEnumerable const&>(*this).begin();
        }
        iterator_t end()
        {
            return static_cast<GeneratedEnumerable const&>(*this).end();
        }
    public:  // copy/move semantics
        explicit GeneratedEnumerable(typename iterator_t::generator_t&& func)
        : func_{std::move(func)}
        {}
        explicit GeneratedEnumerable(typename iterator_t::generator_t const& func)
        : func_{func}
        {}
        GeneratedEnumerable(GeneratedEnumerable const& other) = default;
        GeneratedEnumerable(GeneratedEnumerable&& other) = default;
        GeneratedEnumerable& operator=(GeneratedEnumerable const& other) = delete;
        GeneratedEnumerable& operator=(GeneratedEnumerable&& other) = delete;
    private:  // members
        typename iterator_t::generator_t func_;

#ifdef PYDEF
public:  // python
        template <typename PY>
        static PY def(PY& c)
        {
            using T = typename PY::type;
            c.def(py::init<typename iterator_t::generator_t const&>());
            c.def("__iter__", [](ptr_t<T> e){ return py::make_iterator(e->begin(), e->end()); },
                  py::keep_alive<0, 1>());
            return c;
        }
#endif  // PYDEF
    };

}}  // namespace extensions::iter

#endif  // EXTENSIONS_ITER_GENERATED_ITERATOR_H
