#include "../system.h"
using namespace extensions;
#include "../iter/generated_iterator.h"
using namespace extensions::iter;
#include "mesh.h"
#include "tree.h"
#include "multitree.h"
#include "property_tree.h"
#include "children_traversal.h"
using namespace extensions::mesh;

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    auto c = py::class_<ChildrenTraversal, ptr_t<ChildrenTraversal>>(m, "ChildrenTraversal");
    ChildrenTraversal::def(c);
}
