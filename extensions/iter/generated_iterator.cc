#include "../system.h"
using namespace extensions;
#include "generated_iterator.h"
using namespace extensions::iter;
#include "../mesh/mesh.h"
using namespace extensions::mesh;

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    {
        using iter_t = GeneratedEnumerable<int>;
        auto c = py::class_<iter_t, ptr_t<iter_t>>(m, "GeneratedIterableInt");
        iter_t::def(c);
    }
    {
        using iter_t = GeneratedEnumerable<Mesh::mesh_ptr>;
        auto c = py::class_<iter_t, ptr_t<iter_t>>(m, "GeneratedIterableMesh");
        iter_t::def(c);
    }
    {
        using iter_t = GeneratedEnumerable<torch::Tensor>;
        auto c = py::class_<iter_t, ptr_t<iter_t>>(m, "GeneratedIterableTensor");
        iter_t::def(c);
    }
 }
