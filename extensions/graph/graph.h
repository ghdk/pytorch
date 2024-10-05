#ifndef EXTENSIONS_GRAPH_GRAPH_H
#define EXTENSIONS_GRAPH_GRAPH_H

namespace extensions { namespace graph
{

/**
 * The Graph implements only the APIs for bitmap access and resize. All the
 * graph related algorithms are external. All the types needed to capture
 * different types of graphs, vertices and edges, are implicitly represented
 * through an index and a collection of feature sets.
 */
struct Graph
{

public:

    operator std::string();

    using vertices_visitor_t = storage::vertices_visitor_t;
    void vertices(vertices_visitor_t visitor, size_t start = 0, size_t stop = 0, size_t step = 1);
    void vertices(vertices_visitor_t visitor, size_t start = 0, size_t stop = 0, size_t step = 1) const;

    using edges_visitor_t = storage::edges_visitor_t;
    void edges(edges_visitor_t visitor, size_t start = 0, size_t stop = 0, size_t step = 1);
    void edges(edges_visitor_t visitor, size_t start = 0, size_t stop = 0, size_t step = 1) const;

    bool vertex(feature::index_t i);
    feature::index_t vertex(feature::index_t index, bool truth);
    bool edge(feature::index_t i, feature::index_t j);
    void edge(feature::index_t i, feature::index_t j, bool truth);

public:
    static Graph make_graph()
    {
        Graph ret;
        ret.storage_ = std::make_shared<storage::storage_t>(storage::Memory());
        return ret;
    }

    static Graph make_graph(graphdb::schema::TransactionNode& parent, graph::feature::index_t graph)
    {
        Graph ret;
        ret.storage_ = std::make_shared<storage::storage_t>(storage::Database(parent, graph));
        return ret;
    }

private:
    Graph()
    : storage_{nullptr}
    {}

public:
    Graph(Graph const& other) = default;
    Graph(Graph&& other) = default;
    Graph& operator=(Graph const& other) = default;
    Graph& operator=(Graph&& other) = delete;

private:
    storage::storage_ptr_t storage_;

#ifdef PYDEF
public:
    template <typename PY>
    static PY def(PY& c)
    {
        using T = typename PY::type;
        c.def("__repr__", +[](ptr_t<Graph> self){return (std::string)*self;});
        c.def("__str__", +[](ptr_t<Graph> self){return (std::string)*self;});
        c.def("vertices", static_cast<void(T::*)(typename T::vertices_visitor_t, size_t, size_t, size_t)>(&T::vertices));
        c.def("edges",    static_cast<void(T::*)(typename T::edges_visitor_t, size_t, size_t, size_t)>(&T::edges));
        c.def("is_vertex", static_cast<bool(T::*)(feature::index_t)>(&T::vertex));
        c.def("vertex", static_cast<feature::index_t(T::*)(feature::index_t,bool)>(&T::vertex));
        c.def("is_edge", static_cast<bool(T::*)(feature::index_t,feature::index_t)>(&T::edge));
        c.def("edge", static_cast<void(T::*)(feature::index_t,feature::index_t,bool)>(&T::edge));
        return c;
    }
#endif  // PYDEF
    
};

}}  // namespace extensions::graph

#endif  // EXTENSIONS_GRAPH_GRAPH_H
