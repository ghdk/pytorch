#include "../system.h"
using namespace extensions;
#include "feature.h"
using namespace extensions::graph::feature;
#include "graph.h"
using namespace extensions::graph;
#include "../bitarray/bitarray.h"
using namespace extensions::bitarray;

size_t extensions::graph::feature::order(const Graph graph)
{
    size_t ret = 0;
    auto vertices = graph.vertices();
    size_t max = bitarray::size(vertices);
    for(size_t i = 0; i < max; ++i)
        ret += bitarray::get(vertices, i) ? 1 : 0;
    return ret;
}

size_t extensions::graph::feature::size(const Graph graph, bool directed)
{
    size_t ret = 0;
    auto vertices = graph.vertices().accessor<bitarray::cell_t, 1UL>();
    auto edges = graph.edges().accessor<bitarray::cell_t, 2UL>();
    size_t max = bitarray::size(vertices);
    for(size_t i = 0; i < max; ++i)
    {
        if(!bitarray::get(vertices, i)) continue;
        auto row = edges[i];
        for(size_t cell_i = 0; cell_i < row.size(0); ++cell_i)
        {
            if(!directed && i / cellsize < cell_i) continue;
            if(!row[cell_i]) continue;
            for(size_t j = 0; j < bitarray::cellsize; ++j)
            {
                if(!directed && i < cell_i * bitarray::cellsize + j) continue;
                ret += bitarray::get(row, cell_i * bitarray::cellsize + j);
            }
        }
    }
    return ret;
}

void extensions::graph::feature::FEATURE_PYBIND11_MODULE_IMPL(py::module_ m)
{
    m.attr("DIRECTED") = pybind11::int_(feature::directed);
    m.attr("UNDIRECTED") = pybind11::int_(feature::undirected);

    m.def("order", &extensions::graph::feature::order);
    m.def("size", &extensions::graph::feature::size);
}
