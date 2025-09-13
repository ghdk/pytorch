#ifndef EXTENSIONS_GRAPHDB_SCHEMA_H
#define EXTENSIONS_GRAPHDB_SCHEMA_H

/**
 * A Graph DB built on LMDB. LMDB has the following constraints:
 *
 * - LMDB keys are limited to 511 bytes, see mdb_env_get_maxkeysize.
 * - A low number of named DBs per file is preferable, see mdb_env_set_maxdbs.
 * - Direct modification of DB buffers is not allowed, see MDB_val.
 * - Values are limited to 4GiB, see MDB_val.
 * - Keys are lexicographically sorted, the cursor might not return keys in
 *   numerical order.
 * - One transaction, plus its children, per thread. Preferable one writer,
 *   many readers per process.
 * - One environment per database per process.
 * - Large write transactions cause the DB file to grow significantly larger
 *   than the size of the data. The errors MDB_MAP_FULL or MDB_TXN_FULL suggest
 *   large write transactions. On memory utilisation see
 *   `<https://www.symas.com/post/understanding-lmdb-database-file-sizes-and-memory-utilization>`_
 *
 * Larger buffers are split into PAGE_SIZE chunks before they are stored
 * in the DB. Hence we need to associate a single key with multiple data chunks.
 * DUPSORT doesn't help in this case, as each chunk would have to be a tuple
 * <index, chunk> and this embedding would require more memory copies.
 *
 * We do not use DUPSORT. Keys that need to have multiple values form linked
 * lists of the form
 *
 * <head, n> : <value>
 *
 * or for hashes
 *
 * <hash, n> : key in DB
 *
 * For example lets consider the entry
 *
 * 0 : "ba5e" "ba11" "15" "c001"
 *
 * This would result in the linked list
 *
 * 0,0: "ba5e"
 * 0,1: "ba11"
 * 0,2: "15"
 * 0,3: "c001"
 *
 * Hashing named DBs will also need more than one values per key, due to hash
 * collisions. For the hashing function we will use std::hash with
 * std::string_view.
 *
 * The DB layout is driven by the in memory representation of a graph.
 *
 * ```
    class graph
    {
        struct vertex_features
        {
            std::vector<STRING> token;
            std::vector<STRING> filename;

            std::unordered_multimap<HASH, INDEX> filename_index;
        };

        struct edge_features
        {
            std::unordered_map<INDEX, INTEGER> cost;
        };

        Tensor vertex_set;
        Tensor adjacency_matrix;
    }
 * ```
 *
 * In DB this representation has a direct translation into the following
 * schema.
 *
 * ```
    Named DB: Vertex Features, "VF"
    Key: size_t GRAPH INDEX, size_t VERTEX INDEX, size_t ATTRIBUTE
    Value: linked list head in Linked Lists

    Named DB: Edge Features, "EF"
    Key: size_t GRAPH INDEX, size_t EDGE INDEX, size_t ATTRIBUTE
    Value: linked list head in Linked Lists

    Named DB: Graph Features, "GF"
    Key: size_t GRAPH INDEX, size_t ATTRIBUTE
    Value: linked list head in Linked Lists

    Named DB: Adjacency Matrix, "AM"
    Key: size_t GRAPH INDEX, size_t VERTEX INDEX (0-n)
    Value: linked list head in Linked Lists

    Named DB: Vertex Set, "VS"
    Key: size_t GRAPH INDEX  (0-n)
    Value: linked list head in Linked Lists
 * ```
 *
 * Where `EDGE INDEX` is a single size_t index, set to the index of the edge in
 * the flattened adjacency matrix. Since the VERTEX and EDGE indexes are of
 * the same type, size_t, if we had max size_t vertices we would need
 * max size_t^2 edges. Hence we would need two size_t variables to capture the
 * edges. In that case however, transformations such as a line graph, would
 * require the line graph to have 128bit vertex indexes, and 2x128bit edge
 * indexes. This introduces a recursion resulting in growing index types.
 *
 * Instead we bound the EDGE index to size_t and we capture within each graph
 * sqrt(max size_t) vertices. Since in the DB we can have multiple graphs,
 * each of these graphs become effectively the induced subgraphs of a larger
 * graph with > sqrt(max size_t) vertices, ie. assuming that we have a graph
 * G with a vertex set size [0, 2x sqrt(max size_t)], the induced subgraphs
 * would be
 *
 * G1 [0, sqrt(max size_t) -1] and
 * G2 [sqrt(max size_t), 2x sqrt(max size_t)]
 *
 * If we wanted to express edges between G1 and G2, we would need a new graph
 * G' whose vertices refer to vertices from G1 and G2, and whose edges have one
 * end in G1 and one end in G2, effectively making G' a bipartite graph. When
 * G is fully connected and so the vertices of G1 are fully connected to the
 * vertices of G2, we would need 4 halves of G1 and G2, and so 4x G' graphs each
 * covering a pair of those halves. The edges of each G' would connect the
 * vertices from different halves.
 *
 * One further detail that we need to consider is that the EDGE index refers to
 * a flattened adjacency matrix. On the other hand the vertex set is of variable
 * size as it expands with vertex additions. Hence if we were using the current
 * size of the vertex set to calculate the EDGE index, these indexes would get
 * invalidated every time we add a vertex. As such all EDGE indexes are
 * calculated based on the max size of the vertex set, ie. sqrt(max size_t).
 *
 * `ATTRIBUTE` is the numerical representation of the feature, not its hash,
 * ex. the key `<1,2,'t'>` in `VF` translates to graph=1, vertex=2,
 * feature=token.
 *
 * Internal named DBs.
 *
 * ```
    Named DB: Hash Table, "HVF", "HEF", "HGF"
    Key: size_t HASH, size_t TAIL
    Value: ref

    Named DB: Linked Lists, "LVF", "LEF", "LGF", "LAM", "LVS"
    Key: size_t HEAD, size_t TAIL
    Value: domain specific value of size <= page size
 * ```
 *
 * The `Hash Table` maps hashes to DB keys, the whole buffer should
 * be hashed, not the chunks, as the hash points at the head of each
 * linked list.
 *
 * For each feature table `VF`, `EF`, `GF`, there is a corresponding hash
 * table `HVF`, `HEF`, `HGF` and a linked list table `LVF`, `LEF`, `LGF`.
 * Feature tables hold keys pointing to linked list heads. Linked list tables
 * hold the data, each key pointing to a page. This redirection allows us
 * to have multiple references to the same data. Hash tables and linked list
 * tables form linked lists, the only difference being that on hash tables
 * the head of the list is <HASH,0> while on linked lists is <RANDOM,0>.
 *
 * The readers of the DB need to know in advance the size of the buffer that
 * they are about to read, so that they can allocate sufficient memory. Linked
 * lists end with the special node <head, int max> which holds the length of
 * the list and the size of the buffer that is held.
 *
 * We can have more than one graphs in the DB, each graph identified by the
 * `GRAPH INDEX`.
 *
 */

namespace extensions { namespace graphdb
{

namespace flags
{
    namespace db
    {
        constexpr unsigned int WRITE = MDB_CREATE;
        constexpr unsigned int READ  = 0;
    }

    namespace txn
    {
        constexpr unsigned int WRITE = 0x0;
        constexpr unsigned int READ  = MDB_RDONLY;
        constexpr unsigned int NESTED_RW = 0x0 | 0x80000000;  // only write transactions can be nested
        constexpr unsigned int NESTED_RO = 0x0 | 0x40000000;
    }

    namespace env
    {
        constexpr unsigned int DEFAULT = MDB_NOSUBDIR | MDB_NOMEMINIT | MDB_NOSYNC | MDB_NOMETASYNC;
    }

    namespace put
    {
        constexpr unsigned int DEFAULT = 0;
        constexpr unsigned int NOOVERWRITE = MDB_NOOVERWRITE;
    }
}  // namespace flags

namespace schema
{

constexpr int page_size = 4096;  // bytes

#define T_(t)  t, "H" t, "L" t
constexpr std::array SCHEMA = {T_("VF"),
                               T_("EF"),
                               T_("GF"),
                               T_("AM"),
                               T_("VS"),
                               (const char*)nullptr};  // the unnamed DB

constexpr size_t H(const size_t n)
{
    assertm(0 == n % 3 && n < SCHEMA.size(), n);
    return n+1;
}
constexpr size_t L(const size_t n)
{
    assertm(0 == n % 3 && n < SCHEMA.size(), n);
    return n+2;
}

constexpr size_t VERTEX_FEATURE     = 0;
constexpr size_t VERTEX_FEATURE_H   = H(VERTEX_FEATURE);
constexpr size_t VERTEX_FEATURE_L   = L(VERTEX_FEATURE);
constexpr size_t EDGE_FEATURE       = 3;
constexpr size_t EDGE_FEATURE_H     = H(EDGE_FEATURE);
constexpr size_t EDGE_FEATURE_L     = L(EDGE_FEATURE);
constexpr size_t GRAPH_FEATURE      = 6;
constexpr size_t GRAPH_FEATURE_H    = H(GRAPH_FEATURE);
constexpr size_t GRAPH_FEATURE_L    = L(GRAPH_FEATURE);
constexpr size_t ADJACENCY_MATRIX   = 9;
constexpr size_t ADJACENCY_MATRIX_H = H(ADJACENCY_MATRIX);
constexpr size_t ADJACENCY_MATRIX_L = L(ADJACENCY_MATRIX);
constexpr size_t VERTEX_SET         = 12;
constexpr size_t VERTEX_SET_H       = H(VERTEX_SET);
constexpr size_t VERTEX_SET_L       = L(VERTEX_SET);
constexpr size_t UNNAMED            = 15;

template<typename K, size_t N=1, typename = std::enable_if_t<1 <= N && N <= 3>>
struct key_t
{
public:
    using value_type = K;
private:
    /**
     * [1,3]: keys of the feature tables
     * [2]: keys of the linked list tables
     */
    std::array<value_type, N> data_ = {0};
public:

    constexpr size_t size() { return data_.size(); }
    constexpr size_t size() const { return const_cast<key_t*>(this)->size(); }

    bool operator==(key_t const& other) noexcept
    {
        return data_ == other.data_;
    }
    bool operator==(key_t const& other) const noexcept { return const_cast<key_t*>(this)->operator==(other); }

    bool operator!=(key_t const& other) noexcept
    {
        return !(*this == other);
    }
    bool operator!=(key_t const& other) const noexcept { return const_cast<key_t*>(this)->operator!=(other); }

    value_type* data() noexcept
    {
        return data_.data();
    }
    const value_type* data() const noexcept { return const_cast<key_t*>(this)->data(); }

    value_type& graph(){ return data_[0]; }
    value_type const& graph() const noexcept { return const_cast<key_t*>(this)->graph(); }

    template <size_t M = N, typename = std::enable_if_t<M >= 2>>
    value_type& vertex()
    {
        return data_[1];
    }

    template <size_t M = N, typename = std::enable_if_t<M == 3>>
    value_type& edge()
    {
        return data_[1];
    }

    template <size_t M = N, typename = std::enable_if_t<M >= 2>>
    value_type& attribute()
    {
        return data_[N - 1];
    }

public:  // linked list

    template <size_t M = N, typename = std::enable_if_t<M == 2>>
    value_type& head()
    {
        return data_[0];
    }
    template <size_t M = N, typename = std::enable_if_t<M == 2>>
    value_type const& head() const { return const_cast<key_t*>(this)->head(); }

    template <size_t M = N, typename = std::enable_if_t<M == 2>>
    value_type& tail()
    {
        return data_[1];
    }
    template <size_t M = N, typename = std::enable_if_t<M == 2>>
    value_type const& tail() const { return const_cast<key_t*>(this)->tail(); }

public:
    key_t() = default;
    key_t(key_t const&) = default;
    key_t(key_t&&) = default;
    key_t& operator=(key_t const&) = default;
    key_t& operator=(key_t&&) = default;

    constexpr key_t(std::initializer_list<value_type> list)
    {
        assertm(list.size() == data_.size(), list.size(), data_.size());

        size_t index = 0;
        for(auto i : list)
        {
            data_[index] = i;
            index++;
        }
    }
};

/*
 * The design decision is for the key_t to be a type whose size can be obtained
 * through sizeof. This allows us to instantiate an mdb_view of key_t as
 * mdb_view<key_t>(&key,1) or
 * mdb_view<typename key_t::value_type>(key.data(), key.size()).
 *
 * The following definitions also confirm the correct size of the various
 * key_t instantiations.
 */

using single_key_t = std::enable_if_t<sizeof(key_t<size_t, 1>) == 1 * sizeof(size_t), key_t<size_t, 1>>;
using double_key_t = std::enable_if_t<sizeof(key_t<size_t, 2>) == 2 * sizeof(size_t), key_t<size_t, 2>>;
using triple_key_t = std::enable_if_t<sizeof(key_t<size_t, 3>) == 3 * sizeof(size_t), key_t<size_t, 3>>;

using list_key_t           = double_key_t;
using vertex_feature_key_t = triple_key_t;
using edge_feature_key_t   = triple_key_t;
using graph_feature_key_t  = double_key_t;
using graph_adj_mtx_key_t  = double_key_t;
using graph_vtx_set_key_t  = single_key_t;

struct list_end_  // use through list_end_t
{
public:
    using value_type = list_key_t::value_type;
private:
    /**
     * [0]: the number of nodes in the list
     * [1]: the size of the buffer held by the list
     * [2]: hash
     * [3]: refcount
     */
    std::array<value_type, 4> data_ = {0};

public:

    constexpr size_t size() { return data_.size(); }
    constexpr size_t size() const { return const_cast<list_end_*>(this)->size(); }

    value_type* data() noexcept { return data_.data(); }
    const value_type* data() const noexcept { return const_cast<list_end_*>(this)->data(); }

public:  // accessors
    value_type& length()
    {
        assert(0 < data_.size());
        return data_[0];
    }
    const value_type& length() const noexcept { return const_cast<list_end_*>(this)->length(); }

    value_type& bytes()
    {
        assert(1 < data_.size());
        return data_[1];
    }
    const value_type& bytes() const noexcept { return const_cast<list_end_*>(this)->bytes(); }

    value_type& hash()
    {
        assert(2 < data_.size());
        return data_[2];
    }
    const value_type& hash() const noexcept { return const_cast<list_end_*>(this)->hash(); }

    value_type& refcount()
    {
        assert(3 < data_.size());
        return data_[3];
    }
    const value_type& refcount() const noexcept { return const_cast<list_end_*>(this)->refcount(); }

public:
    list_end_() = default;
    list_end_(list_end_ const&) = default;
    list_end_(list_end_&&) = default;
    list_end_& operator=(list_end_ const&) = default;
    list_end_& operator=(list_end_&&) = default;

    constexpr list_end_(std::initializer_list<value_type> list)
    {
        assertm(list.size() == data_.size(), list.size(), data_.size());

        size_t index = 0;
        for(auto i : list)
        {
            data_[index] = i;
            index++;
        }
    }
};

using list_end_t = std::enable_if_t<sizeof(list_end_) == 4 * sizeof(typename list_end_::value_type), list_end_>;

using meta_key_t = list_key_t;
using meta_page_t = meta_key_t::value_type;

// We reserve head=0, a) no hash value should be 0, b) linked list head
// allocations are random, hence collisions on 0 do not matter. The meta link
// list does not end in a special end node, as the list is hard coded.
constexpr typename meta_key_t::value_type RESERVED = 0ULL;
const meta_key_t META_KEY_PAGE = {RESERVED, 0x0ULL};
constexpr auto LIST_TAIL_MAX = std::numeric_limits<typename list_key_t::value_type>::max();

template<typename K>
using is_list_key_t = std::enable_if_t<std::is_same_v<K, list_key_t>, void>;

template<typename K>
using is_not_list_key_t = std::enable_if_t<!std::is_same_v<K, list_key_t>, void>;

template<typename K>
using is_key_t = std::enable_if_t<std::is_constructible_v<std::variant<vertex_feature_key_t, // covers edge_feature_key_t
                                                                       graph_feature_key_t,  // covers graph_adj_mtx_key_t, list_key_t
                                                                       graph_vtx_set_key_t>,
                                                          K>, void>;

template<typename K>
using is_graph_vtx_set_key_t = std::enable_if_t<std::is_same_v<K, graph_vtx_set_key_t>, void>;

template<typename K>
using is_graph_adj_mtx_key_t = std::enable_if_t<std::is_same_v<K, graph_adj_mtx_key_t>, void>;

template<typename K>
using is_feature_key_t = std::enable_if_t<std::is_constructible_v<std::variant<vertex_feature_key_t, // covers edge_feature_key_t
                                                                               graph_feature_key_t>,
                                                                  K>, void>;

inline std::ostream& operator<<(std::ostream& strm, list_end_t const& end)
{
    std::ostringstream out;
    for(size_t i = 0; i < end.size(); i++)
        out << (i ? "," : "") << end.data()[i];
    return strm << out.str();
}

template<typename K, typename = is_key_t<K>>
std::ostream& operator<<(std::ostream &strm, K const& key)
{
    std::ostringstream out;
    for(size_t i = 0; i < key.size(); i++)
        out << (i ? "," : "") << key.data()[i];
    return strm << out.str();
}

}  // namespace schema

}}  // namespace extensions::graphdb

#endif  // EXTENSIONS_GRAPHDB_SCHEMA_H
