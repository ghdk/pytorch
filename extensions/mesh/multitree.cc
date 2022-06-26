#include "../system.h"
using namespace extensions;
#include "mesh.h"
#include "tree.h"
#include "multitree.h"
using namespace extensions::mesh;

namespace extensions { namespace mesh { namespace multitree_tests
{
    
    class X
        : public MultiTree
    {};

    class Y
        : public MultiTree
    {};

    class Z
        : public MultiTree
    {};

    class Test{};

}}}  // namespace extensions::mesh::multitree_tests

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    auto t = py::class_<MultiTree, Tree, ptr_t<MultiTree>>(m, "MultiTree");
    Mesh::def(t);
    MultiTree::def(t);

    {
        using namespace extensions::mesh::multitree_tests;
        auto ts = py::class_<Test, ptr_t<Test>>(m, "Test");

        auto x = py::class_<X, Mesh, ptr_t<X>>(ts, "X");
        Mesh::def(x);
        auto y = py::class_<Y, Mesh, ptr_t<Y>>(ts, "Y");
        Mesh::def(y);
        auto z = py::class_<Z, Mesh, ptr_t<Z>>(ts, "Z");
        Mesh::def(z);
    }

 }
