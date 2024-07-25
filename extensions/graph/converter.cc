#include "../system.h"
using namespace extensions;
#include "graph.h"
using namespace extensions::graph;
#include "converter.h"
#include "../bitarray/bitarray.h"

Graph extensions::graph::converter::add(Graph const& g, torch::Tensor vertex)
{
    Graph ret;
    std::cerr << "vertices=" << g.vertices().sizes() << " edges=" << g.edges().sizes() << std::endl;
    std::cerr << "vertex=" << vertex.dim() << " sizes=" << vertex.sizes() << std::endl;
    assertm(1 == vertex.dim(), "");
    auto& vertices = ret.vertices();
    auto& edges = ret.edges();
    vertices= torch::full({g.vertices().sizes()[0] + 1, std::max(g.vertices().sizes()[1], vertex.sizes()[0])},
                          extensions::graph::UNDEF,
                          g.vertices().options());
    if(g.vertices().sizes()[0])
        vertices.slice(0 /* rows */,
                       0 /* start row */,
                       g.vertices().sizes()[0] /* end row */)
                .slice(1 /* columns */,
                       0 /* start column */,
                       g.vertices().sizes()[1] /* end column */) = g.vertices().slice(0, 0, g.vertices().sizes()[0])
                                                                               .slice(1, 0, g.vertices().sizes()[1]);
    vertices[-1] = vertex;
    edges = torch::full({vertices.sizes()[0], ((long) (vertices.sizes()[0] / bitsize<extensions::bitarray::cell_t>)) + 1L},
                        0,
                        g.edges().options());
    if(g.edges().sizes()[0])
        edges.slice(0 /* rows */,
                    0 /* start row */,
                    g.edges().sizes()[0] /* end row */)
             .slice(1 /* columns */,
                    0 /* start column */,
                    g.edges().sizes()[1] /* end column */) = g.edges().slice(0, 0, g.edges().sizes()[0])
                                                                      .slice(1, 0, g.edges().sizes()[1]);
    std::cerr << "vertices=" << vertices.sizes() << " edges=" << edges.sizes() << std::endl;
    return ret;
}

Graph extensions::graph::converter::remove(Graph& g, size_t index)
{
    Graph ret;
    auto& vertices = ret.vertices();
    auto& edges = ret.edges();
    if(index >= g.vertices().sizes()[0])
        return ret;  // graph is empty, or index is out of bounds
    vertices = torch::full({g.vertices().sizes()[0] - 1, g.vertices().sizes()[1]},
                           extensions::graph::UNDEF,
                           g.vertices().options());
    edges = torch::full({g.edges().sizes()[0] - 1, g.edges().sizes()[1]},
                        extensions::graph::UNDEF,
                        g.edges().options());
    // NOTE: we never reduce the columns, we would need to verify that
    // all remaining vertices have UNDEF for last index.
    vertices.slice(0 /* rows */,
                   0 /* start row */,
                   index /* end row: get the vertices in the range [0,index) */)
            .slice(1 /* columns */,
                   0 /* start column */,
                   g.vertices().sizes()[1] /* end column */) = g.vertices().slice(0, 0, index)
                                                                           .slice(1, 0, g.vertices().sizes()[1]);
    vertices.slice(0 /* rows */,
                   index /* start row */,
                   vertices.sizes()[0] /* end row: get the vertices in the range (index, end] */)
            .slice(1 /* columns */,
                   0 /* start column */,
                   g.vertices().sizes()[1] /* end column */) = g.vertices().slice(0, index + 1, g.vertices().sizes()[0])
                                                                           .slice(1,         0, g.vertices().sizes()[1]);
    // Reduce the edges by one row, do not reduce the columns.
    edges.slice(0 /* rows */,
                0 /* start row */,
                index /* end row: get the edges in the range [0,index) */)
         .slice(1 /* columns */,
                0 /* start column */,
                g.edges().sizes()[1] /* end column */) = g.edges().slice(0, 0, index)
                                                                  .slice(1, 0, g.edges().sizes()[1]);
    edges.slice(0 /* rows */,
                index /* start row */,
                edges.sizes()[0] /* end row: get the edges in the range (index, end] */)
         .slice(1 /* columns */,
                0 /* start column */,
                g.edges().sizes()[1] /* end column */) = g.edges().slice(0, index + 1, g.edges().sizes()[0])
                                                                  .slice(1,         0, g.edges().sizes()[1]);
    // Shift to the left by one, the edges of vertices >= index.
    for(size_t row = 0; row < edges.sizes()[0]; row++)
    {
        auto tensor_of_row = edges[row];
        for(size_t column = index; column < g.edges().sizes()[0]; column++)
        {
            torch::Tensor truth = (column + 1 < g.edges().sizes()[0]
                                   ? extensions::bitarray::get(tensor_of_row, column + 1)
                                   : torch::full(1, false));
            extensions::bitarray::set(tensor_of_row, column, truth.item<bool>());
        }
    }
    return ret;
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    m.def("add_vertex", &extensions::graph::converter::add);
    m.def("rm_vertex", &extensions::graph::converter::remove);
}
