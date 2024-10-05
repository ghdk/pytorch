extern "C"
{
#include "lmdb.h"
}

#include "../system.h"
#include "../bitarray/bitarray.h"
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
    this->vertices([&](feature::index_t v){ ret += std::to_string(v) + ", "; });
    ret += "\nE=";
    this->edges([&](feature::index_t e1, feature::index_t e2)
                {
                    ret += "(" + std::to_string(e1) + "," + std::to_string(e2) + ")" + ", ";
                });
    ret += "\n";
    return ret;
}

void extensions::graph::Graph::vertices(vertices_visitor_t visitor, size_t start, size_t stop, size_t step)
{
    storage_->vertices(visitor, start, stop, step);
}
void extensions::graph::Graph::edges(edges_visitor_t visitor, size_t start, size_t stop, size_t step)
{
    storage_->edges(visitor, start, stop, step);
}

void extensions::graph::Graph::vertices(vertices_visitor_t visitor, size_t start, size_t stop, size_t step) const
{
    const_cast<Graph*>(this)->vertices(visitor, start, stop, step);
}
void extensions::graph::Graph::edges(edges_visitor_t visitor, size_t start, size_t stop, size_t step) const
{
    const_cast<Graph*>(this)->edges(visitor, start, stop, step);
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
    m.def("make_graph_db", +[](extensions::ptr_t<extensions::graphdb::schema::TransactionNode> parent, extensions::graph::feature::index_t graph)
          { return extensions::graph::Graph::make_graph(*parent, graph); });
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    PYBIND11_MODULE_IMPL(m);
    extensions::graph::feature::FEATURE_PYBIND11_MODULE_IMPL(m);
}
