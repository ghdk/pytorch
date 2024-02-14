#ifndef EXTENSIONS_GRAPH_GRAPH_H
#define EXTENSIONS_GRAPH_GRAPH_H

namespace extensions { namespace graph
{

static constexpr int64_t UNDEF = -1LL;

struct Graph
{

public:  // copy/move semantics
    Graph()
    {
        auto vopt = torch::TensorOptions().dtype(torch::kInt64)
                                          .requires_grad(false);
        vertices_ = torch::zeros(0, vopt);
        auto eopt = torch::TensorOptions().dtype(torch::kUInt8)
                                          .requires_grad(false);
        edges_    = torch::zeros(0, eopt);
    }
    Graph(Graph const& other) = delete;
    Graph(Graph&& other) = default;
    Graph& operator=(Graph const& other) = delete;
    Graph& operator=(Graph&& other) = delete;

    operator std::string()
    {
        return std::string("V=") + torch::str(vertices_) + std::string("\nE=") + torch::str(edges_);
    }

    torch::Tensor& vertices()
    {
        assertm(vertices_.sizes()[0] == edges_.sizes()[0], "");
        return vertices_;
    }

    torch::Tensor& edges()
    {
        assertm(vertices_.sizes()[0] == edges_.sizes()[0], "");
        return edges_;
    }

    torch::Tensor const& vertices() const
    {
        return const_cast<Graph*>(this)->vertices();
    }

    torch::Tensor const& edges() const
    {
        return const_cast<Graph*>(this)->edges();
    }
    
private:
    /**
     * The graph is data agnostic, consists of index sets to some container
     * unknown to the graph.
     *
     * The vertex set is a 2d tensor, forming a matrix where rows are vertices
     * and each vertex is a vector. It is the responsibility of an external
     * function to map the indices of a vector to the container(s) holding
     * the data.
     *
     * The edge set is a 2d tensor, each row being a bitmap, forming a matrix
     * that represents the graph. The indices of the bitmap correspond to the
     * indices of some external container holding metadata for the edges.
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
        c.def("vertices", static_cast<torch::Tensor const& (T::*)() const>(&T::vertices));
        c.def("edges",    static_cast<torch::Tensor const& (T::*)() const>(&T::edges));
        return c;
    }
#endif  // PYDEF
    
};

}}  // namespace extensions::graph

#endif  // EXTENSIONS_GRAPH_GRAPH_H
