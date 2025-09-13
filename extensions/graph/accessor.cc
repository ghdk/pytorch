#include "../system.h"
using namespace extensions;
#include "graph.h"
using namespace extensions::graph;
#include "accessor.h"
#include "../bitarray/bitarray.h"

Graph& extensions::graph::accessor::add(Graph& g, std::array<size_t,2> indices)
{
    auto row = g.edges()[indices[0]];
    extensions::bitarray::set(row, indices[1], true);
    return g;
}

torch::Tensor extensions::graph::accessor::edge(Graph const& g, std::array<size_t,2> indices)
{
    auto row = g.edges()[indices[0]];
    return extensions::bitarray::get(row, indices[1]);
}

torch::Tensor extensions::graph::accessor::vertex(Graph const& g, size_t vertex)
{
    auto v = g.vertices()[vertex];
    return v;
}

Graph& extensions::graph::accessor::remove(Graph& g, std::array<size_t,2> indices)
{
    auto row = g.edges()[indices[0]];
    extensions::bitarray::set(row, indices[1], false);
    return g;
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    m.def("add_edge", &extensions::graph::accessor::add);
    m.def("edge", &extensions::graph::accessor::edge);
    m.def("vertex", &extensions::graph::accessor::vertex);
    m.def("rm_edge", &extensions::graph::accessor::remove);
}
