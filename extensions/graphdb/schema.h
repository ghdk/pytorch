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
 * Feature tables hold keys pointing to linked list heads. Linked list tables
 * hold the data, each key pointing to a page. This redirection allows us
 * to have multiple references to the same data. Hash tables and linked list
 * tables form linked lists, the only difference being that on hash tables
 * the head of the list is <HASH,0> while on linked lists is <RANDOM,0>.
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
        constexpr unsigned int DEFAULT = MDB_NOSUBDIR;
    }

    namespace put
    {
        constexpr unsigned int DEFAULT = 0;
        constexpr unsigned int NOOVERWRITE = MDB_NOOVERWRITE;
    }
}  // namespace flags

namespace schema
{

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

struct DatabaseSet
{
public:
    graphdb::Database main_;
    graphdb::Database hash_;
    graphdb::Database list_;

public:
    DatabaseSet(graphdb::Transaction const& txn, DatabaseSet const& parent)
    : main_{txn, parent.main_}
    , hash_{txn, parent.hash_}
    , list_{txn, parent.list_}
    {}

    DatabaseSet(graphdb::Transaction const& txn, size_t db, unsigned int flags)
    : main_{txn, txn.env_.schema_[db],    flags}
    , hash_{txn, txn.env_.schema_[H(db)], flags}
    , list_{txn, txn.env_.schema_[L(db)], flags}
    {}

    DatabaseSet() = delete;
    DatabaseSet(DatabaseSet const&) = delete;
    DatabaseSet(DatabaseSet&&) = delete;
    DatabaseSet& operator=(DatabaseSet const&) = delete;
    DatabaseSet& operator=(DatabaseSet&&) = delete;
};

/**
 * It is the responsibility of the caller to manage the database nodes, not the
 * callee. Imagine a caller that has a root rw database node. The caller wants
 * to call a function to read only a feature. In this case the caller prepares
 * a new child, ro, database node and passes to the callee the database set. The
 * callee does not create a new transaction, instead uses the transaction and
 * database from the database set. Hence the transactions are always at the
 * database node level.
 */
struct TransactionNode
{
public:
    graphdb::Environment const& env_;
    graphdb::Transaction txn_;

public:
    DatabaseSet vertex_;
    DatabaseSet edge_;
    DatabaseSet graph_;
    DatabaseSet adj_mtx_;
    DatabaseSet vtx_set_;

public:
    TransactionNode(graphdb::Environment const& env, unsigned int flags)  // create a root node
    : env_{env}
    , txn_{env_, flags}
    , vertex_(txn_,  VERTEX_FEATURE,   extensions::graphdb::flags::db::WRITE)
    , edge_(txn_,    EDGE_FEATURE,     extensions::graphdb::flags::db::WRITE)
    , graph_(txn_,   GRAPH_FEATURE,    extensions::graphdb::flags::db::WRITE)
    , adj_mtx_(txn_, ADJACENCY_MATRIX, extensions::graphdb::flags::db::WRITE)
    , vtx_set_(txn_, VERTEX_SET,       extensions::graphdb::flags::db::WRITE)
    , flags_{flags}
    {}

    TransactionNode(TransactionNode const& parent, unsigned int flags)  // create a child node
    : env_{parent.env_}
    , txn_{parent.env_, parent.txn_, flags}
    , vertex_(txn_,  parent.vertex_)
    , edge_(txn_,    parent.edge_)
    , graph_(txn_,   parent.graph_)
    , adj_mtx_(txn_, parent.adj_mtx_)
    , vtx_set_(txn_, parent.vtx_set_)
    , flags_{flags}
    {}

    ~TransactionNode()
    {
        int rc;
        switch(flags_)
        {
        case extensions::graphdb::flags::txn::WRITE:
        case extensions::graphdb::flags::txn::NESTED_RW:
            assertm(MDB_SUCCESS == (rc = txn_.commit()), rc);
            break;
        case extensions::graphdb::flags::txn::READ:
        case extensions::graphdb::flags::txn::NESTED_RO:
            assertm(MDB_SUCCESS == (rc = txn_.abort()), rc);
            break;
        default:
            assertm(false, flags_);
        }
    }

    TransactionNode() = delete;
    TransactionNode(TransactionNode const&) = delete;
    TransactionNode(TransactionNode&&) = delete;
    TransactionNode& operator=(TransactionNode const&) = delete;
    TransactionNode& operator=(TransactionNode&&) = delete;

private:
    unsigned int flags_;
};

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

template<typename K>
using is_graph_vtx_set_key_t = std::enable_if_t<std::is_same_v<K, graph_vtx_set_key_t>, void>;

template<typename K>
using is_graph_adj_mtx_key_t = std::enable_if_t<std::is_same_v<K, graph_adj_mtx_key_t>, void>;

template<typename K>
using is_feature_key_t = std::enable_if_t<std::is_constructible_v<std::variant<vertex_feature_key_t, // covers edge_feature_key_t
                                                                               graph_feature_key_t>,
                                                                  K>, void>;

template<typename K, typename = is_key_t<K>>
std::ostream& operator<<(std::ostream &strm, K &key)
{
    std::ostringstream out;
    for(size_t i = 0; i < key.size(); i++)
        out << (i ? "," : "") << key.data()[i];
    return strm << out.str();
}

}  // namespace schema

}}  // namespace extensions::graphdb

#endif  // EXTENSIONS_GRAPHDB_SCHEMA_H
