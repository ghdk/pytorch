#include "../system.h"
using namespace extensions;
#include "generated_iterator.h"
#include "mapped_iterator.h"
using namespace extensions::iter;
#include "../mesh/mesh.h"
#include "../mesh/tree.h"
#include "../mesh/multitree.h"
using namespace extensions::mesh;

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    {
        using iter_t = MappedEnumerable<int, GeneratedEnumerable<int>>;
        auto c = py::class_<iter_t, ptr_t<iter_t>>(m, "MappedGeneratedIterableInt");
        iter_t::def(c);
    }
    {
        using iter_t = MappedEnumerable<MultiTree::multitree_ptr, GeneratedEnumerable<Mesh::mesh_ptr>>;
        auto c = py::class_<iter_t, ptr_t<iter_t>>(m, "MappedGeneratedIterableMultitree");
        iter_t::def(c);
    }
}
