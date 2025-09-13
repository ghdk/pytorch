extern "C"
{
#include "lmdb.h"
}

#include "../system.h"
using namespace extensions;
#include "../bitarray/bitarray.h"
#include "../iter/generated_iterator.h"
#include "../iter/iter.h"
#include "feature.h"
#include "../graphdb/db.h"
#include "../graphdb/schema.h"
#include "../graphdb/graphdb.h"
#include "storage.h"
#include "graph.h"

size_t extensions::graph::feature::order(const Graph graph)
{
    size_t ret = 0;
    for(auto v : graph.vertices())
        ret += 1;
    return ret;
}

size_t extensions::graph::feature::size(const Graph graph, bool directed)
{
    size_t ret = 0;
    for(auto e : graph.edges())
    {
        // If the graph is not directed don't count the edges twice.
        if(!directed && std::get<0>(e) > std::get<1>(e)) continue;
        ret += 1;
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
