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

    torch::Tensor vertices();
    torch::Tensor edges();
    const torch::Tensor vertices() const;
    const torch::Tensor edges() const;

    bool vertex(index_t);

    /**
     * - When bool is True, adds a vertex.
     * - When bool is False, removes a vertex.
     *
     * On vertex additions:
     * - When the vertices set at i is False, sets the bit to True.
     * - When the vertices set at i is True, picks a random index
     * r in [0, max index_t), and if there is a collision follows the
     * open addressing algorithm, seeking forward from r to the end
     * of the PAGE holding r. If a 0 bit cannot be found in the range
     * [r, PAGE_SIZE), the graph is expanded by PAGE_SIZE, and the bit
     * in the newly allocated page at r%PAGE_SIZE is set.
     */
    index_t vertex(index_t, bool);

    bool edge(index_t, index_t);
    void edge(index_t, index_t, bool);

public:  // copy/move semantics

    /**
     * A tensor is a thin object holding a pointer to the underlying storage.
     * Hence passing a graph object by copy is fairly cheap, as it will only
     * bump the reference counts of the underlying storage.
     */
    Graph();
    Graph(Graph const& other) = default;
    Graph(Graph&& other) = delete;
    Graph& operator=(Graph const& other) = default;
    Graph& operator=(Graph&& other) = delete;

private:

    /**
     * The graph is data agnostic, consists of bitmaps.
     *
     * The vertices bitmap, identifies the presence of a vertex. Through reallocs
     * and vertex deletions some bits will be zero. It is a 1D tensor.
     *
     * The edges bitmap is an adjacency matrix. It is a 2D tensor.
     */
    torch::Tensor vertices_;
    torch::Tensor edges_;

#ifdef PYDEF
public:
    template <typename PY>
    static PY def(PY& c)
    {
        using T = typename PY::type;
        c.def(py::init<>());
        c.def("__repr__", +[](ptr_t<Graph> self){return (std::string)*self;});
        c.def("__str__", +[](ptr_t<Graph> self){return (std::string)*self;});
        c.def("vertices", static_cast<torch::Tensor (T::*)()>(&T::vertices));
        c.def("edges",    static_cast<torch::Tensor (T::*)()>(&T::edges));
        c.def("is_vertex", static_cast<bool(T::*)(index_t)>(&T::vertex));
        c.def("vertex", static_cast<index_t(T::*)(index_t,bool)>(&T::vertex));
        c.def("is_edge", static_cast<bool(T::*)(index_t,index_t)>(&T::edge));
        c.def("edge", static_cast<void(T::*)(index_t,index_t,bool)>(&T::edge));
        return c;
    }
#endif  // PYDEF
    
};

}}  // namespace extensions::graph

#endif  // EXTENSIONS_GRAPH_GRAPH_H
