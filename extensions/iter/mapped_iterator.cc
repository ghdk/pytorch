#include "../system.h"
using namespace extensions;
#include "iter.h"
#include "generated_iterator.h"
#include "mapped_iterator.h"
using namespace extensions::iter;

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    {
        using iter_t = MappedEnumerable<int, GeneratedEnumerable<int>>;
        auto c = py::class_<iter_t, ptr_t<iter_t>>(m, "MappedGeneratedIterableInt");
        iter_t::def(c);
    }
}
