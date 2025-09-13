#include "../system.h"
using namespace extensions;
#include "graph.h"
using namespace extensions::graph;

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    auto c = py::class_<Graph, ptr_t<Graph>>(m, "Graph");
    Graph::def(c);
}
