#ifndef EXTENSIONS_GRAPH_CONVERTER_H
#define EXTENSIONS_GRAPH_CONVERTER_H

namespace extensions { namespace graph { namespace converter
{
    // Require memory operations, otherwise it is an accessor.
    
    Graph add(Graph const& g, torch::Tensor vertex);
    Graph remove(Graph& g, size_t index);
    
}}}  // namespace extensions::graph::converter

#endif  // EXTENSIONS_GRAPH_CONVERTER_H
