#include "../system.h"
using namespace extensions;
#include "mesh.h"
#include "tree.h"
#include "multitree.h"
#include "property_tree.h"
using namespace extensions::mesh;

namespace extensions { namespace mesh { namespace property_tree_tests
{
    
    class X
        : public PropertyTree
    {};

    class Y
        : public PropertyTree
    {};

    class Z
        : public PropertyTree
    {};

    class Test{};

}}}  // namespace extensions::mesh::property_tree_tests

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    auto t = py::class_<PropertyTree, MultiTree, ptr_t<PropertyTree>>(m, "PropertyTree");
    Mesh::def(t);
    MultiTree::def(t);

    {
        using namespace extensions::mesh::property_tree_tests;
        auto ts = py::class_<Test, ptr_t<Test>>(m, "Test");

        auto x = py::class_<X, MultiTree, ptr_t<X>>(ts, "X");
        Mesh::def(x);
        auto y = py::class_<Y, MultiTree, ptr_t<Y>>(ts, "Y");
        Mesh::def(y);
        auto z = py::class_<Z, MultiTree, ptr_t<Z>>(ts, "Z");
        Mesh::def(z);
    }

 }
