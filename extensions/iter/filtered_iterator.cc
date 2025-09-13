#include "../system.h"
using namespace extensions;
#include "generated_iterator.h"
#include "filtered_iterator.h"
using namespace extensions::iter;
#include "../mesh/mesh.h"
using namespace extensions::mesh;

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    {
        using iter_t = FilteredEnumerable<GeneratedEnumerable<int>>;
        auto c = py::class_<iter_t, ptr_t<iter_t>>(m, "FilteredGeneratedIterableInt");
        iter_t::def(c);
    }
    {
        using iter_t = FilteredEnumerable<GeneratedEnumerable<Mesh::mesh_ptr>>;
        auto c = py::class_<iter_t, ptr_t<iter_t>>(m, "FilteredGeneratedIterableMesh");
        iter_t::def(c);
    }
 }
