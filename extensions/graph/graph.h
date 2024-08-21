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

    iter::GeneratedEnumerable<feature::index_t> vertices();
    iter::GeneratedEnumerable<std::pair<feature::index_t, feature::index_t>> edges();
    iter::GeneratedEnumerable<feature::index_t> vertices() const;
    iter::GeneratedEnumerable<std::pair<feature::index_t, feature::index_t>> edges() const;

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

    static Graph make_graph(graphdb::Environment& env, graph::feature::index_t graph)
    {
        Graph ret;
        ret.storage_ = std::make_shared<storage::storage_t>(storage::Database(env, graph));
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
        c.def("vertices", static_cast<iter::GeneratedEnumerable<feature::index_t>(T::*)(void)>(&T::vertices));
        c.def("edges",    static_cast<iter::GeneratedEnumerable<std::pair<feature::index_t, feature::index_t>>(T::*)(void)>(&T::edges));
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
