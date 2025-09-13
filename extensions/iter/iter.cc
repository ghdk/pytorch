#include "../system.h"
using namespace extensions;
#include "iter.h"
#include "generated_iterator.h"
#include "mapped_iterator.h"
#include "filtered_iterator.h"
#include "enumerated_iterator.h"
using namespace extensions::iter;

struct generator
{
    std::optional<size_t> operator()() const
    {
        return const_cast<generator*>(this)->impl();
    }

    std::optional<size_t> impl()
    {
        std::optional<size_t> ret = {};
        if(index_ < max_) ret = index_;
        index_++;
        return ret;
    }

    size_t index_ = 0;
    static const size_t max_ = 4;
};

void PYBIND11_MODULE_IMPL(py::module_ m)
{
    auto mt = m.def_submodule("test", "");

    mt.def("test_generated_iterator",
           +[]()
           {
                {
                    using array_t = std::array<size_t, generator::max_>;
                    array_t data = {0};
                    for(auto [i,v] : enumerate(generate(generator())))
                        data[i] = v;

                    assertm((data == array_t{0,1,2,3}), data[0], data[1], data[2], data[3]);
                }
           });

    mt.def("test_enumerated_iterator",
           +[]()
           {
                {
                    using array_t = std::array<size_t, generator::max_>;
                    array_t data = {0,1,2,3};
                    for(auto [i,v] : enumerate(data))
                        data[i] = v+1;

                    assertm((data == array_t{1,2,3,4}), data[0], data[1], data[2], data[3]);
                }
           });

    mt.def("test_mapped_iterator",
           +[]()
           {
                {
                    using array_t = std::array<size_t, generator::max_>;
                    array_t data = {0};
                    for(auto [i,v] : enumerate(map(generate(generator()), +[](size_t n){ return n*2; })))
                        data[i] = v;

                    assertm((data == array_t{0,2,4,6}), data[0], data[1], data[2], data[3]);
                }

                {
                    using array_t = std::array<size_t, generator::max_>;
                    static_assert(std::is_pointer_v<array_t::iterator>);
                    static_assert(std::is_pointer_v<array_t::const_iterator>);
                    array_t data = {0};
                    for(auto [i,v] : enumerate(map(data, +[](size_t n){ return n+1; })))
                        data[i] = v;

                    assertm((data == array_t{1,1,1,1}), data[0], data[1], data[2], data[3]);
                }
           });

    mt.def("test_filtered_iterator",
           +[]()
           {
                {
                    using array_t = std::array<size_t, generator::max_>;
                    array_t data = {0};
                    for(auto [i,v] : enumerate(filter(generate(generator()), +[](size_t n){ return 0 == n%2; })))
                        data[i] = v;

                    assertm((data == array_t{0,2,0,0}), data[0], data[1], data[2], data[3]);
                }

                {
                    using array_t = std::array<size_t, generator::max_>;
                    static_assert(std::is_pointer_v<array_t::iterator>);
                    static_assert(std::is_pointer_v<array_t::const_iterator>);
                    array_t data = {0,1,2,3};
                    for(auto [i,v] : enumerate(filter(data, +[](size_t n){ return 1 == n%2; })))
                        data[i] = v;

                    assertm((data == array_t{1,3,2,3}), data[0], data[1], data[2], data[3]);
                    //                           ^^^ left unchanged
                }
           });

}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    PYBIND11_MODULE_IMPL(m);
}
