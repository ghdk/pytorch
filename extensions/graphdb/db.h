#ifndef EXTENSIONS_GRAPHDB_DATABASE_H
#define EXTENSIONS_GRAPHDB_DATABASE_H

namespace extensions { namespace graphdb
{

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

    mdb_view(const_iterator begin, const size_t count)
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

template<typename T>
struct is_mdb_view: std::false_type {};
template<typename T>
struct is_mdb_view<mdb_view<T>> : std::true_type {};

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
    Environment(std::string const& filename, schema_t schema, unsigned int flags)
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

        rc = mdb_env_open(handle_, filename_.data(), flags, 0600);
        assertm(MDB_SUCCESS == rc, rc);
    }

    ~Environment()
    {
        assert(nullptr != handle_);
        mdb_env_close(handle_);
        handle_ = nullptr;
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
        assert(nullptr != handle_);
        int rc = mdb_txn_commit(handle_);
        handle_ = nullptr;
        return rc;
    }

    int abort()
    {
        assert(nullptr != handle_);
        mdb_txn_abort(handle_);
        handle_ = nullptr;
        return MDB_SUCCESS;
    }
public:
    Transaction(Environment const& env, unsigned int flags)
    : env_{env}
    {
        int rc = mdb_txn_begin(env_, NULL, flags, &handle_);
        assert(MDB_SUCCESS == rc);
    }

    /**
     * In order to create child transactions do not a) create a child
     * transaction, and b) reopen the database. Instead pass to the callee the
     * parent database and a) create a child transaction from the parent's
     * reference to the transaction, and b) create a new database object from
     * the parent database. This way the child database will not attempt to
     * close the database handle.
     */

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

public:
    Transaction(Transaction const&) = delete;
    Transaction(Transaction&&) = delete;
    Transaction& operator=(Transaction const&) = delete;
    Transaction& operator=(Transaction&&) = delete;

public:
    handle_type handle_;
    Environment const& env_;
};

class Database
{
public:
    using handle_type = MDB_dbi;
    using view_type = MDB_val*;
private:
    template<typename K, typename V>
    using constraint_t = std::enable_if_t<   !is_mdb_view<K>::value
                                          && !is_mdb_view<V>::value
                                          && !std::is_same_v<view_type, K>
                                          && !std::is_same_v<view_type, V>>;
public:
    int put(const view_type key, const view_type value, unsigned int flags)
    {
        int rc = mdb_put(txn_, handle_, key, value, flags);
        return rc;
    }

    template <typename K, typename V, typename = constraint_t<K,V>>
    int put(K const& key, V const& data, unsigned int flags)
    {
        int rc = 0;

        static_assert(extensions::is_contiguous<K>::value);
        mdb_view<typename K::value_type> key_v(key.data(), key.size());

        if constexpr(std::is_same_v<V, torch::Tensor>)
        {
            torch::Tensor tensor = data;
            if(torch::kFloat == tensor.dtype())
            {
                mdb_view<float> data_v(tensor.data_ptr<float>(), tensor.numel());
                rc = put(key_v, data_v, flags);
            }
            else if(torch::kUInt8 == tensor.dtype())
            {
                mdb_view<uint8_t> data_v(tensor.data_ptr<uint8_t>(), tensor.numel());
                rc = put(key_v, data_v, flags);
            }
            else
                assertm(false, tensor.dtype());
        }
        else if constexpr(extensions::is_contiguous<V>::value)
        {
            mdb_view<typename V::value_type> data_v(data.data(), data.size());
            rc = put(key_v, data_v, flags);
        }
        else if constexpr(std::is_fundamental_v<V>)
        {
            mdb_view<V> data_v(&data, 1);
            rc = put(key_v, data_v, flags);
        }
        else
            static_assert(extensions::false_always<K,V>::value);
        return rc;
    }

    int get(const view_type key, view_type data)
    {
        return mdb_get(txn_, handle_, key, data);
    }

    template <typename K, typename V, typename = constraint_t<K,V>>
    int get(K const& key, V& data)
    {
        int rc = 0;

        static_assert(extensions::is_contiguous<K>::value);
        mdb_view<typename K::value_type> key_v(key.data(), key.size());

        if constexpr(std::is_same_v<V, torch::Tensor>)
        {
            auto lambda = [&](auto view)
            {
                // NB. Quoting "Exposes the given data as a Tensor without taking ownership of the original data."
                torch::Tensor blob = torch::from_blob(view.begin(), data.sizes(), data.options());
                data.copy_(blob);  // take ownership
            };

            if(torch::kFloat == data.dtype())
            {
                mdb_view<float> data_v;
                rc = get(key_v, data_v);
                if(MDB_SUCCESS == rc)
                    lambda(data_v);
            }
            else if(torch::kUInt8 == data.dtype())
            {
                mdb_view<uint8_t> data_v;
                rc = get(key_v, data_v);
                if(MDB_SUCCESS == rc)
                    lambda(data_v);
            }
            else
                assertm(false, data.dtype());
        }
        else if constexpr(extensions::is_contiguous<V>::value)
        {
            mdb_view<typename V::value_type> data_v(data.data(), data.size());
            rc = get(key_v, data_v);
            if(MDB_SUCCESS == rc)
                memcpy(data.data(), data_v.begin(), data_v.size() * sizeof(typename V::value_type));
        }
        else if constexpr(std::is_fundamental_v<V>)
        {
            mdb_view<V> data_v(&data, 1);
            rc = get(key_v, data_v);
            if(MDB_SUCCESS == rc)
                data = *(data_v.begin());
        }
        else
            static_assert(extensions::false_always<K,V>::value);
        return rc;
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
    int put(const view_type key, const view_type value, unsigned int flags) const
    {
        return const_cast<Database*>(this)->put(key, value, flags);
    }
    template <typename K, typename V, typename = constraint_t<K,V>>
    int put(K const& key, V const& value, unsigned int flags) const
    {
        return const_cast<Database*>(this)->put(key, value, flags);
    }
    int get(const view_type key, view_type data) const
    {
        return const_cast<Database*>(this)->get(key, data);
    }
    template <typename K, typename V, typename = constraint_t<K,V>>
    int get(K const& key, V& data) const
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
    using view_type = MDB_val*;
private:
    template<typename K, typename V>
    using constraint_t = std::enable_if_t<   !is_mdb_view<K>::value
                                          && !is_mdb_view<V>::value
                                          && !std::is_same_v<view_type, K>
                                          && !std::is_same_v<view_type, V>>;

public:
    int get(view_type key, view_type value, MDB_cursor_op op)
    {
        return mdb_cursor_get(handle_, key, value, op);
    }

    template <typename K, typename V, typename = constraint_t<K,V>>
    int get(K& key, V& value, MDB_cursor_op op)
    {
        int rc = 0;

        static_assert(extensions::is_contiguous<K>::value);
        mdb_view<typename K::value_type> key_v(key.data(), key.size());

        if constexpr(extensions::is_contiguous<V>::value)
        {
            mdb_view<typename V::value_type> value_v(value.data(), value.size());
            rc = get(key_v, value_v, op);
            if(MDB_SUCCESS == rc)
            {
                if(key_v.data() != key.data())
                    memcpy(key.data(), key_v.begin(), key_v.size() * sizeof(typename K::value_type));
                memcpy(value.data(), value_v.begin(), value_v.size() * sizeof(typename V::value_type));
            }
        }
        else
            static_assert(extensions::false_always<K,V>::value);
        return rc;
    }

public:
    Cursor(Transaction const& txn, Database const& db)
    : txn_{txn}
    , db_{db}
    {
        int rc = mdb_cursor_open(txn_, db_, &handle_);
        assertm(MDB_SUCCESS == rc, rc);
    }

    ~Cursor()
    {
        assert(nullptr != handle_);
        assert(nullptr != txn_.handle_);
        mdb_cursor_close(handle_);
    }

    Cursor() = delete;
    Cursor(Cursor const&) = delete;
    Cursor(Cursor&&) = delete;

    handle_type handle_;
    Transaction const& txn_;
    Database const& db_;
};

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
