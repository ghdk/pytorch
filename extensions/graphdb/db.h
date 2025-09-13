#ifndef EXTENSIONS_GRAPHDB_DATABASE_H
#define EXTENSIONS_GRAPHDB_DATABASE_H

namespace extensions { namespace graphdb
{

class Environment
{
public:
    using handle_type = MDB_env*;
    using schema_t = gsl::span<const char * const>;
public:
    int max_key_size()
    {
        return mdb_env_get_maxkeysize(handle_);
    }
    size_t cardinality()
    {
        MDB_stat stat;
        int rc = mdb_env_stat(handle_, &stat);
        assert(MDB_SUCCESS == rc);
        return stat.ms_entries;
    }
public:
    Environment(std::string const& filename, schema_t schema)
    : filename_{filename}
    , schema_{std::move(schema)}
    {
        int rc = -1;
        rc = mdb_env_create(&handle_);
        assertm(MDB_SUCCESS == rc, rc);

        const size_t maxdbs = schema_.size() + 1;  // ie. 1 for unnamed + n named
        rc = mdb_env_set_maxdbs(handle_, maxdbs);
        assertm(MDB_SUCCESS == rc, rc);

        const size_t maxsz = 1ULL * 1024 * 1024 * 1024 * 1024;  // 1 TiB
        rc = mdb_env_set_mapsize(handle_, maxsz);
        assertm(MDB_SUCCESS == rc, rc);

        rc = mdb_env_open(handle_, filename_.data(), flags::env::DEFAULT, 0600);
        assertm(MDB_SUCCESS == rc, rc);
    }

    ~Environment()
    {
        mdb_env_close(handle_);
    }

    operator handle_type() const
    {
        return handle_;
    }

public:
    std::string filename_;
    schema_t schema_;
private:
    handle_type handle_;
};

class Transaction
{
public:
    using handle_type = MDB_txn*;
public:
    int commit()
    {
        int rc = mdb_txn_commit(handle_);
        return rc;
    }

    int abort()
    {
        mdb_txn_abort(handle_);
        return MDB_SUCCESS;
    }
public:
    Transaction(Environment const& env, unsigned int flags)
    : env_{env}
    {
        int rc = mdb_txn_begin(env_, NULL, flags, &handle_);
        assert(MDB_SUCCESS == rc);
    }

    Transaction(Environment const& env, Transaction const& parent, unsigned int flags)
    : env_{env}
    {
        int rc = mdb_txn_begin(env_, parent, flags, &handle_);
        assertm(MDB_SUCCESS == rc, rc);
    }

    operator handle_type() const
    {
        return handle_;
    }

    handle_type handle_;
    Environment const& env_;
};

class Database
{
public:
    using handle_type = MDB_dbi;
    using view_type = MDB_val*;
public:
    int put(view_type key, view_type value, unsigned int flags)
    {
        int rc = mdb_put(txn_, handle_, key, value, flags);
        return rc;
    }
    int get(view_type key, view_type data)
    {
        return mdb_get(txn_, handle_, key, data);
    }
    int remove(view_type key)
    {
        return mdb_del(txn_, handle_, key, NULL);
    }
    size_t cardinality()
    {
        MDB_stat stat;
        int rc = mdb_stat(txn_, handle_, &stat);
        assert(MDB_SUCCESS == rc);
        return stat.ms_entries;
    }
public:  // const
    int put(view_type key, view_type value, unsigned int flags) const
    {
        return const_cast<Database*>(this)->put(key, value, flags);
    }
    int get(view_type key, view_type data) const
    {
        return const_cast<Database*>(this)->get(key, data);
    }
    int remove(view_type key) const
    {
        return const_cast<Database*>(this)->remove(key);
    }
    size_t cardinality() const
    {
        return const_cast<Database*>(this)->cardinality();
    }
public:
    Database(Transaction const& txn, Database const& parent)
    : txn_{txn}
    , handle_{parent.handle_}
    {
        close_ = false;
    }
    Database(Transaction const& txn, std::string_view name, unsigned int flags)
    : txn_{txn}
    {
        int rc = mdb_dbi_open(txn, !name.empty() ? name.data() : NULL, flags, &handle_);
        assertm(MDB_SUCCESS == rc, rc);
    }

    ~Database()
    {
        /**
         * Quoting the LMDB docs
         *
         * The database handle will be private to the current transaction until
         * the transaction is successfully committed. If the transaction is
         * aborted the handle will be closed automatically. After a successful
         * commit the handle will reside in the shared environment, and may be
         * used by other transactions.
         *
         * [...]
         *
         * Closing a database handle is not necessary, but lets mdb_dbi_open()
         * reuse the handle value. Usually it's better to set a bigger
         * mdb_env_set_maxdbs(), unless that value would be large.
         */

        if(close_)
            mdb_dbi_close(txn_.env_, handle_);
    }

    operator handle_type() const
    {
        return handle_;
    }

    Transaction const& txn_;
    handle_type handle_;
private:
    bool close_ = true;
};

class Cursor
{
public:
    using handle_type = MDB_cursor*;

    /**
     * mdb_cursor_get (MDB_cursor *cursor, MDB_val *key, MDB_val *data, MDB_cursor_op op)
     * mdb_cursor_put (MDB_cursor *cursor, MDB_val *key, MDB_val *data, unsigned int flags)
     * mdb_cursor_del (MDB_cursor *cursor, unsigned int flags)
     *
     */

public:
    Cursor(Transaction const& txn, Database const& db)
    : txn_{txn}
    , db_{db}
    {
        int rc = mdb_cursor_open(txn_.handle_, db_.handle_, &handle_);
        assertm(MDB_SUCCESS == rc, rc);
    }

    ~Cursor()
    {
        mdb_cursor_close(handle_);
    }

    Cursor() = delete;
    Cursor(Cursor const&) = delete;
    Cursor(Cursor&&) = delete;

    handle_type handle_;
    Transaction const& txn_;
    Database const& db_;
};

template<typename T>
class mdb_view
{
public:
    using handle_type = MDB_val;
    using value_type = T;
    using const_iterator = T const*;
    using iterator = T*;

public:

    mdb_view()
    {
        handle_.mv_data = nullptr;
        handle_.mv_size = 0;
    }
    mdb_view(MDB_val const& other)
    {
        handle_.mv_data = other.mv_data;
        handle_.mv_size = other.mv_size;
    }

    mdb_view(const_iterator begin, size_t count)
    : mdb_view(const_cast<iterator>(begin), count)
    {}

    mdb_view(iterator begin, size_t count)
    {
        handle_.mv_data = begin;
        handle_.mv_size = sizeof(value_type) * count;
    }

    mdb_view(mdb_view const&) = default;
    mdb_view(mdb_view&&) = default;
    mdb_view& operator=(mdb_view const&) = default;
    mdb_view& operator=(mdb_view&&) = default;

public:

    constexpr const_iterator data() const noexcept
    {
        return static_cast<const_iterator>(handle_.mv_data);
    }

    constexpr size_t size() const noexcept
    {
        return handle_.mv_size / sizeof(value_type);
    }

    const_iterator begin() const
    {
        return static_cast<const_iterator>(handle_.mv_data);
    }
    const_iterator end() const
    {
        return static_cast<const_iterator>(handle_.mv_data) + size();
    }

    iterator begin()
    {
        return const_cast<iterator>(static_cast<mdb_view const*>(this)->begin());
    }
    iterator end()
    {
        return const_cast<iterator>(static_cast<mdb_view const*>(this)->end());
    }

public:

    operator handle_type()
    {
        return handle_;
    }

    operator handle_type*()
    {
        return &handle_;
    }

private:
    handle_type handle_;
};

template<typename T>
bool operator==(mdb_view<T> const& lhs, mdb_view<T> const& rhs) noexcept
{
    bool ret = (lhs.end() - lhs.begin() == rhs.end() - rhs.begin());
    for(auto i = lhs.begin(), j = rhs.begin(); i < lhs.end() && j < rhs.end(); ++i, ++j)
    {
        ret &= (*i == *j);
    }
    return ret;
}

template<typename T>
bool operator!=(mdb_view<T> const& lhs, mdb_view<T> const& rhs) noexcept
{
    return !(lhs == rhs);
}

}}  // namespace extensions::graphdb

namespace std
{
template<typename T>
struct hash<extensions::graphdb::mdb_view<T>>
{
    std::size_t operator()(extensions::graphdb::mdb_view<T> const& view)
    {
        std::string_view v{reinterpret_cast<const char*>(view.data()),
                           view.size() * sizeof(typename extensions::graphdb::mdb_view<T>::value_type)};
        return std::hash<std::string_view>{}(v);
    }
};
}
#endif  // EXTENSIONS_GRAPHDB_DATABASE_H
