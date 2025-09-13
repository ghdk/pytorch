#ifndef EXTENSIONS_GRAPHDB_TRANSACTION_H
#define EXTENSIONS_GRAPHDB_TRANSACTION_H

namespace extensions { namespace graphdb
{

namespace schema
{

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
 *
 * On nested transactions:
 *
 * Write transactions can be nested. Child transactions see the uncommitted
 * writes of their parent. The child transaction can commit or abort, at which
 * point its writes become visible to the parent transaction or are discarded.
 * If the parent aborts, all of the writes performed in the context of the
 * parent, including those from committed child transactions, are discarded.
 *
 * src: `<https://github.com/antimer/lmdb>`_
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

class PyTransactionNode
{

    /**
     * The transaction node relies on RAII, and during its destruction closes
     * the transaction and the databases.
     *
     * In Python we cannot rely on object destruction. Instead we expose this
     * object that implements the context manager protocol, and wraps a
     * transaction node in a unique pointer. Python does not gain ownership of
     * the transaction node, hence during __exit__ we can destroy it, and close
     * the transaction through RAII.
     */

    extensions::ptr_u<extensions::graphdb::schema::TransactionNode> txn_;

public:
    extensions::graphdb::schema::TransactionNode& txn()
    {
        return *txn_;
    }

    extensions::graphdb::schema::TransactionNode const& txn() const
    {
        return const_cast<PyTransactionNode*>(this)->txn();
    }

public:
    PyTransactionNode(std::string filename)
    {
        extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);
        txn_ = std::make_unique<extensions::graphdb::schema::TransactionNode>(std::ref(env), extensions::graphdb::flags::txn::NESTED_RW);
    }

#ifdef PYDEF
public:
    template <typename PY>
    static PY def(PY& c)
    {
        using T = typename PY::type;
        c.def(py::init<std::string>());
        c.def("__enter__", +[](extensions::ptr_t<PyTransactionNode> self) {
            assert(nullptr != self->txn_);
            return self;
        });
        c.def("__exit__",  +[](extensions::ptr_t<PyTransactionNode> self,
                               std::optional<pybind11::type> const&exc_type,
                               std::optional<pybind11::object> const& exc_value,
                               std::optional<pybind11::object> const& traceback){
            assert(nullptr != self->txn_);
            self->txn_ = nullptr;
        });
        return c;
    }
#endif  // PYDEF

};

}  // namespace schema

}}  // namespace extensions::graphdb

#endif  // EXTENSIONS_GRAPHDB_TRANSACTION_H
