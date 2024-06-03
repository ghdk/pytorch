#ifndef EXTENSIONS_GRAPHDB_SCHEMA_H
#define EXTENSIONS_GRAPHDB_SCHEMA_H

/**
 * LMDB has the following constraints:
 *
 * - LMDB keys are limited to 511 bytes, see mdb_env_get_maxkeysize.
 * - A low number of named DBs per file is preferable, see mdb_env_set_maxdbs.
 * - Direct modification of DB buffers is not allowed, see MDB_val.
 * - Values are limited to 4GiB, see MDB_val.
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
 * Where `ATTRIBUTE` is the numerical representation of the feature,
 * not its hash, ex. the key `<1,2,'t'>` in `VF` translates to graph=1, vertex=2,
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
 * Linked list tables point to a page of data. Hash tables and linked list
 * tables form linked lists, the only difference being that on hash tables
 * the head of the list is <HASH,0> while on linked lists is <RANDOM,0>.
 *
 * We can have more than one graphs in the DB, each graph identified by the
 * `GRAPH INDEX`.
 *
 */

namespace extensions { namespace graphdb
{

namespace schema
{

#define T_(t)  t, "H" t, "L" t
constexpr std::array SCHEMA = {T_("VF"),
                               T_("EF"),
                               T_("GF"),
                               T_("AM"),
                               T_("VS")};

constexpr size_t VERTEX_FEATURE = 0;
constexpr size_t EDGE_FEATURE = 3;
constexpr size_t GRAPH_FEATURE = 6;
constexpr size_t ADJACENCY_MATRIX = 9;
constexpr size_t VERTEX_SET = 12;

template <size_t T> constexpr size_t H = T+1;
template <size_t T> constexpr size_t L = T+2;

template<typename K, size_t N=1>
struct key_t
{
public:
    using value_type = K;
private:
    /**
     * [1,3]: keys of the feature tables
     * [2]: keys of the linked list tables
     */
    static_assert(1 <= N && N <= 3);
    std::array<value_type, N> data_ = {0};
public:

    bool operator==(key_t const& other) noexcept
    {
        return data_ == other.data_;
    }

    bool operator!=(key_t const& other) noexcept
    {
        return !(*this == other);
    }

    value_type* data() noexcept
    {
        return data_.data();
    }

    value_type& graph(){ return data_[0]; }

    value_type& vertex()
    {
        static_assert(N >= 2);
        return data_[1];
    }

    value_type& edge()
    {
        static_assert(N == 3);
        return data_[1];
    }

    value_type& attribute()
    {
        static_assert(N >= 2);
        return data_[N - 1];
    }

public:  // linked list

    value_type& head()
    {
        static_assert(N == 2);
        return data_[0];
    }

    value_type& tail()
    {
        static_assert(N == 2);
        return data_[1];
    }

public:
    key_t() = default;
    key_t(key_t const&) = default;
    key_t(key_t&&) = default;
    key_t& operator=(key_t const&) = default;
    key_t& operator=(key_t&&) = default;

    key_t(std::initializer_list<value_type> list)
    {
        assertm(list.size() == data_.size(), list.size(), data_.size());

        size_t index = 0;
        for(auto i : list)
        {
            data_[index] = i;
            index++;
        }
    }

public:  // const
    bool operator==(key_t const& other) const noexcept
    {
        return const_cast<key_t*>(this)->operator==(other);
    }
    bool operator!=(key_t const& other) const noexcept
    {
        return const_cast<key_t*>(this)->operator!=(other);
    }
    value_type const& head() const
    {
        return const_cast<key_t*>(this)->head();
    }
    value_type const& tail() const
    {
        return const_cast<key_t*>(this)->tail();
    }
};

using list_key_t = key_t<graph::feature::hash_t, 2>;

using vertex_feature_key_t = key_t<graph::feature::index_t, 3>;
using edge_feature_key_t = key_t<graph::feature::index_t, 3>;
using graph_feature_key_t = key_t<graph::feature::index_t, 2>;
using graph_adj_mtx_key_t = key_t<graph::feature::index_t, 2>;
using graph_vtx_set_key_t = key_t<graph::feature::index_t, 1>;

static_assert(sizeof(list_key_t) == 2 * sizeof(typename list_key_t::value_type));  // Ensure key has no padding.

using meta_key_t = list_key_t;
using meta_page_t = meta_key_t::value_type;

// We reserve 0, a) no hash value should be 0, b) linked list head allocations
// are random, hence collisions on 0 do not matter.
constexpr typename meta_key_t::value_type RESERVED = 0ULL;
const meta_key_t META_KEY_PAGE = {RESERVED, 0x0ULL};
constexpr auto LIST_TAIL_MAX = std::numeric_limits<typename list_key_t::value_type>::max();

template<typename K>
using is_list_key_t = std::enable_if_t<std::is_same_v<K, list_key_t>, void>;

template<typename K>
using is_key_t = std::enable_if_t<std::is_constructible_v<std::variant<vertex_feature_key_t, // covers edge_feature_key_t
                                                                       graph_feature_key_t,  // covers graph_adj_mtx_key_t, list_key_t
                                                                       graph_vtx_set_key_t>,
                                                          K>, void>;

}  // namespace schema

namespace flags
{
    namespace db
    {
        constexpr unsigned int WRITE = MDB_CREATE;
        constexpr unsigned int READ  = 0;
    }

    namespace txn
    {
        constexpr unsigned int WRITE = 0;
        constexpr unsigned int READ  = MDB_RDONLY;
        constexpr unsigned int NESTED = WRITE;  // only write transactions can be nested
    }

    namespace env
    {
        constexpr unsigned int DEFAULT = MDB_NOSUBDIR;
    }

    namespace put
    {
        constexpr unsigned int DEFAULT = 0;
    }
}  // namespace flags

}}  // namespace extensions::graphdb

#endif  // EXTENSIONS_GRAPHDB_SCHEMA_H
