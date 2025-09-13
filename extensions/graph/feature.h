#ifndef EXTENSIONS_GRAPH_FEATURE_H
#define EXTENSIONS_GRAPH_FEATURE_H

namespace extensions { namespace graph { struct Graph; }}

namespace extensions { namespace graph { namespace feature
{

/**
 * The graph consists of two bitmaps, the vertex set bitmap and the
 * adjacency matrix. There is no type representing vertices or edges.
 * The type index_t inherits different meanings based on the feature set
 * is used on. Expects that indexes never get invalidated, otherwise all
 * the feature sets that use the same index would need to be updated.
 */
using index_t = size_t;
using hash_t = size_t;

using graph_sparse_set = std::unordered_map<index_t, Graph>;

template <typename T> using feature_set = std::vector<T>;
template <typename T> using feature_sparse_set = std::unordered_map<index_t, T>;
using index_sparse_set = std::unordered_multimap<hash_t, index_t>;

constexpr bool undirected = false;
constexpr bool directed = true;

/**
 * The size of the graph is calculated differently depending on whether the
 * graph is directed or not. Hence it depends on a feature set. The order
 * does not depend on a feature set, but lives here to keep the Graph
 * implementation minimal.
 */
VISIBLE size_t order(Graph graph);  // #vertices
VISIBLE size_t size(Graph graph, bool directed);  // #edges

void FEATURE_PYBIND11_MODULE_IMPL(py::module_ m);

}}}  // namespace extensions::graph::feature

#endif  // EXTENSIONS_GRAPH_FEATURE_H
