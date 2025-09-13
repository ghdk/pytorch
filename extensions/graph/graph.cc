#include "../system.h"
using namespace extensions;
#include "feature.h"
using namespace extensions::graph::feature;
#include "graph.h"
using namespace extensions::graph;
#include "../bitarray/bitarray.h"
using namespace extensions::bitarray;

Graph::Graph()
{
    auto vopt = torch::TensorOptions().dtype(torch::kUInt8)
                                      .requires_grad(false);
    vertices_ = torch::zeros(extensions::page_size, vopt);
    auto eopt = torch::TensorOptions().dtype(torch::kUInt8)
                                      .requires_grad(false);
    edges_    = torch::zeros({extensions::page_size * bitarray::cellsize, extensions::page_size}, eopt);
}

Graph::operator std::string()
{
    return std::string("V=") + torch::str(vertices_) + std::string("\nE=") + torch::str(edges_);
}

torch::Tensor Graph::vertices()
{
    assertm(bitarray::size(vertices_) == edges_.sizes()[0], "");
    return vertices_;
}

torch::Tensor Graph::edges()
{
    assertm(bitarray::size(vertices_) == edges_.sizes()[0], "");
    return edges_;
}

const torch::Tensor Graph::vertices() const
{
    return const_cast<Graph*>(this)->vertices();
}

const torch::Tensor Graph::edges() const
{
    return const_cast<Graph*>(this)->edges();
}

bool Graph::vertex(index_t i)
{
    return get(vertices_, i);
}
index_t Graph::vertex(index_t index, bool truth)
{
    const index_t max = bitarray::size(vertices_) - 1;

    assertm(max >= index, "Index ", index, " is out of bounds [0,", max, ").");
    index_t ret = index;

    if(truth)
    {
        bool needs_expansion = false;
        index_t step = 0;

        std::random_device r;
        std::default_random_engine e(r());
        std::uniform_int_distribution<index_t> d(0, max);

        while(true)
        {
            if(!get(vertices_, ret + step)) break;
            if((ret || step) && !((ret + step) % extensions::page_size))
            {
                needs_expansion = true;
                step = 0;
                ret = max + 1 + (ret % extensions::page_size);
                break;
            }
            if(ret == index)
            {
                ret = d(e);
                step = 0;
            }
            else
                step += 1;
        }
        if(needs_expansion)
        {
            torch::Tensor vertices = torch::full(vertices_.sizes()[0] + extensions::page_size,
                                                 0,
                                                 vertices_.options());
            torch::Tensor edges = torch::full({bitarray::size(vertices), vertices.sizes()[0]},
                                              0,
                                              edges_.options());
            vertices.slice(0 /* rows */,
                           0 /* start row */,
                           vertices_.sizes()[0] /* end row */) = vertices_.slice(0, 0, vertices_.sizes()[0]);
            edges.slice(0 /* rows */,
                        0 /* start row */,
                        edges_.sizes()[0] /* end row */)
                 .slice(1 /* columns */,
                        0 /* start column */,
                        edges_.sizes()[1] /* end column */) = edges_.slice(0, 0, edges_.sizes()[0])
                                                                    .slice(1, 0, edges_.sizes()[1]);
            vertices_ = vertices;
            edges_ = edges;
        }
        ret = ret + step;
        set(vertices_, ret, truth);
    }
    else  // !truth
    {
        set(vertices_, ret, truth);
        auto adjm = edges_.accessor<bitarray::cell_t, 2>();
        for(size_t i = 0; i < bitarray::size(vertices_); ++i)
        {
            if(i == ret)
            {
                for(size_t k = 0; k < adjm[i].size(0); ++k)
                    adjm[i][k] = 0;
            }
            else
                set(adjm[i], ret, truth);
        }
    }

    return ret;
}
bool Graph::edge(index_t i, index_t j)
{
    return get(edges_[i], j);
}

void Graph::edge(index_t i, index_t j, bool truth)
{
    auto tensor = edges_[i];
    set(tensor, j, truth);
}

void PYBIND11_MODULE_IMPL(py::module_ m)
{
    m.attr("PAGE_SIZE") = pybind11::int_(extensions::page_size);
    auto c = py::class_<Graph, ptr_t<Graph>>(m, "Graph");
    Graph::def(c);
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    PYBIND11_MODULE_IMPL(m);
    FEATURE_PYBIND11_MODULE_IMPL(m);
}
