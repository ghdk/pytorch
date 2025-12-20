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

    using vertices_visitor_t = std::function<void(feature::index_t)>;
    using edges_visitor_t = std::function<void(feature::index_t, feature::index_t)>;

    VISIBLE void vertices(vertices_visitor_t visitor, size_t start = 0, size_t stop = 0, size_t step = 1);
    void vertices(vertices_visitor_t visitor, size_t start = 0, size_t stop = 0, size_t step = 1) const;

    VISIBLE void edges(edges_visitor_t visitor, size_t start = 0, size_t stop = 0, size_t step = 1);
    void edges(edges_visitor_t visitor, size_t start = 0, size_t stop = 0, size_t step = 1) const;

    VISIBLE bool vertex(feature::index_t i);
    VISIBLE feature::index_t vertex(feature::index_t index, bool truth);
    VISIBLE bool edge(feature::index_t i, feature::index_t j);
    VISIBLE void edge(feature::index_t i, feature::index_t j, bool truth);

public:
    Graph()
    {
        auto vopt = torch::TensorOptions().dtype(torch::kUInt8)
                                          .requires_grad(false);
        vertices_ = torch::zeros(extensions::graphdb::schema::page_size, vopt);
        auto eopt = torch::TensorOptions().dtype(torch::kUInt8)
                                          .requires_grad(false);
        edges_    = torch::zeros({extensions::graphdb::schema::page_size * bitarray::cellsize, extensions::graphdb::schema::page_size}, eopt);
    }

    Graph(Graph const& other) = default;
    Graph(Graph&& other) = default;
    Graph& operator=(Graph const& other) = default;
    Graph& operator=(Graph&& other) = delete;

private:

    /**
     * A tensor is a thin object holding a pointer to the underlying storage.
     * Hence passing a graph object by copy is fairly cheap, as it will only
     * bump the reference counts of the underlying storage.
     */
    static_assert(8 == sizeof(torch::Tensor));

    /**
     * The graph is data agnostic, consists of bitmaps.
     *
     * The vertices bitmap, identifies the presence of a vertex. Through reallocs
     * and vertex deletions some bits will be zero. It is a 1D tensor.
     *
     * The edges bitmap is an adjacency matrix. It is a 2D tensor.
     * 
     * This storage type keeps the full bitmaps in memory.
     */
    torch::Tensor vertices_;
    torch::Tensor edges_;

public:
    friend void extensions::graphdb::graph::read(extensions::graphdb::Environment& env,
                                                 extensions::graph::Graph graph,
                                                 extensions::graphdb::schema::graph_vtx_set_key_t graph_i);
    friend void extensions::graphdb::graph::write(extensions::graphdb::Environment& env,
                                                  extensions::graph::Graph graph,
                                                  extensions::graphdb::schema::graph_vtx_set_key_t graph_i);

#ifdef PYDEF
public:
    template <typename PY>
    static PY def(PY& c)
    {
        using T = typename PY::type;
        c.def(py::init<>());
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
