#include "../system.h"
using namespace extensions;
#include "mesh.h"
using namespace extensions::mesh;

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    auto c = py::class_<Mesh, ptr_t<Mesh>>(m, "Mesh");
    Mesh::def(c);

    py::enum_<typename Mesh::dim::e>(m, "dim")
        .value("p", Mesh::dim::e::p)
        .value("x", Mesh::dim::e::x)
        .value("y", Mesh::dim::e::y)
        .value("z", Mesh::dim::e::z)
        .value("h", Mesh::dim::e::h)
        ;
}
