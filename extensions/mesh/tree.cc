#include "../system.h"
using namespace extensions;
#include "mesh.h"
#include "tree.h"
using namespace extensions::mesh;

namespace extensions { namespace mesh { namespace tree_tests
{
    
    class X
        : public Tree
    {};

    class Y
        : public Tree
    {};

    class Z
        : public Tree
    {};

    class Test{};

}}}  // namespace extensions::mesh::tree_tests

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    auto t = py::class_<Tree, Mesh, ptr_t<Tree>>(m, "Tree");
    Mesh::def(t);

    {
        using namespace extensions::mesh::tree_tests;
        auto ts = py::class_<Test, ptr_t<Test>>(m, "Test");

        auto x = py::class_<X, Mesh, ptr_t<X>>(ts, "X");
        Mesh::def(x);
        auto y = py::class_<Y, Mesh, ptr_t<Y>>(ts,"Y");
        Mesh::def(y);
        auto z = py::class_<Z, Mesh, ptr_t<Z>>(ts,"Z");
        Mesh::def(z);
    }

 }
