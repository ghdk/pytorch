#include "../system.h"
using namespace extensions;
#include "mesh.h"
#include "star.h"
using namespace extensions::mesh;

namespace extensions { namespace mesh { namespace star_tests
{

    class X
        : public Star
    {};

    class Y
        : public Star
    {};

    class Test{};
}}}  // namespace extensions::mesh::star_tests

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    auto s = py::class_<Star, Mesh, ptr_t<Star>>(m, "Star");
    Mesh::def(s);

    {
        using namespace extensions::mesh::star_tests;
        auto ts = py::class_<Test, ptr_t<Test>>(m, "Test");

        auto x = py::class_<X, Mesh, ptr_t<X>>(ts, "X");
        Mesh::def(x);
        auto y = py::class_<Y, Mesh, ptr_t<Y>>(ts, "Y");
        Mesh::def(y);
    }
}
