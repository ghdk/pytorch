#ifndef EXTENSIONS_GRAPH_ACCESSOR_H
#define EXTENSIONS_GRAPH_ACCESSOR_H

namespace extensions { namespace graph { namespace accessor
{
    // Can modify the graph, but do not require memory operations.
    // Otherwise its a converter.
    
    Graph& add(Graph& g, std::array<size_t,2> indices);
    torch::Tensor edge(Graph const& g, std::array<size_t,2> indices);
    torch::Tensor vertex(Graph const& g, size_t vertex);
    Graph& remove(Graph& g, std::array<size_t,2> indices);
    
}}}  // namespace extensions::graph::accessor

#endif  // EXTENSIONS_GRAPH_ACCESSOR_H
