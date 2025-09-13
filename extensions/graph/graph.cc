extern "C"
{
#include "lmdb.h"
}

#include "../system.h"
#include "../bitarray/bitarray.h"
#include "../iter/generated_iterator.h"
#include "../iter/iter.h"
#include "../graphdb/db.h"
#include "../graphdb/schema.h"
#include "../graphdb/envpool.h"
#include "../graphdb/graphdb.h"
#include "feature.h"
#include "storage.h"
#include "graph.h"

extensions::graph::Graph::operator std::string()
{
    std::string ret = "";
    ret += "V=";
    for(auto v: this->vertices())
        ret += std::to_string(v) + ", ";
    ret += "\nE=";
    for(auto e: this->edges())
        ret += "(" + std::to_string(std::get<0>(e)) + "," + std::to_string(std::get<1>(e)) + ")" + ", ";
    ret += "\n";
    return ret;
}

extensions::iter::GeneratedEnumerable<extensions::graph::feature::index_t> extensions::graph::Graph::vertices()
{
    return storage_->vertices();
}
extensions::iter::GeneratedEnumerable<std::pair<extensions::graph::feature::index_t, extensions::graph::feature::index_t>> extensions::graph::Graph::edges()
{
    return storage_->edges();
}

extensions::iter::GeneratedEnumerable<extensions::graph::feature::index_t> extensions::graph::Graph::vertices() const
{
    return const_cast<Graph*>(this)->vertices();
}
extensions::iter::GeneratedEnumerable<std::pair<extensions::graph::feature::index_t, extensions::graph::feature::index_t>> extensions::graph::Graph::edges() const
{
    return const_cast<Graph*>(this)->edges();
}

bool extensions::graph::Graph::vertex(feature::index_t i)
{
    return storage_->vertex(i);
}
extensions::graph::feature::index_t extensions::graph::Graph::vertex(feature::index_t index, bool truth)
{
    return storage_->vertex(index,truth);
}
bool extensions::graph::Graph::edge(feature::index_t i, feature::index_t j)
{
    return storage_->edge(i,j);
}
void extensions::graph::Graph::edge(feature::index_t i, feature::index_t j, bool truth)
{
    storage_->edge(i,j,truth);
}

void PYBIND11_MODULE_IMPL(py::module_ m)
{
    auto c = py::class_<extensions::graph::Graph, extensions::ptr_t<extensions::graph::Graph>>(m, "Graph");
    extensions::graph::Graph::def(c);
    m.def("make_graph", static_cast<extensions::graph::Graph(*)(void)>(&extensions::graph::Graph::make_graph));
    m.def("make_graph_db", +[](std::string filename, extensions::graph::feature::index_t graph)
          {
            extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);
            return extensions::graph::Graph::make_graph(env, graph);
          });

    {
        using gen_v = extensions::iter::GeneratedEnumerable<extensions::graph::feature::index_t>;
        auto c = py::class_<gen_v, extensions::ptr_t<gen_v>>(m, "GeneratedEnumerableVertex");
        gen_v::def(c);
    }

    {
        using gen_e = extensions::iter::GeneratedEnumerable<std::pair<extensions::graph::feature::index_t, extensions::graph::feature::index_t>>;
        auto c = py::class_<gen_e, extensions::ptr_t<gen_e>>(m, "GeneratedEnumerableEdge");
        gen_e::def(c);
    }
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    PYBIND11_MODULE_IMPL(m);
    extensions::graph::feature::FEATURE_PYBIND11_MODULE_IMPL(m);
}
