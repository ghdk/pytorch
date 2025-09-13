#ifndef EXTENSIONS_GRAPHDB_GRAPHDB_H
#define EXTENSIONS_GRAPHDB_GRAPHDB_H

namespace extensions { namespace graphdb {

namespace list  // Transactions that manage the linked list DBs.
{

namespace head
{

/*
 * Finds the next available slot in the DB to use as a head of a list,
 * following the open addressing algorithm. As a parameter it receives
 * a key to use as a hint for the placement.
 *
 * - If the key is not present in the DB it returns the hint.
 * - If there is a collision, it returns a random number r in
 *   [0, N*PAGE_SIZE*CHAR_BIT), where N is the number of pages.
 * - If there is a collision with r, then seeks forward up to
 *   PAGE_SIZE*CHAR_BIT for an empty slot. If a slot cannot be found,
 *   extends N by one, ie. extends the hypothetical space by one page,
 *   and returns N*PAGE_SIZE*CHAR_BIT + r%(PAGE_SIZE*CHAR_BIT).
 */
template<typename K, typename = schema::is_list_key_t<K>>
void find(graphdb::Database const& parent, graphdb::mdb_view<K> hint)
{
    assertm(0 == hint.begin()->tail(), hint.begin()->tail());

    int rc = 0;

    typename K::value_type ret = hint.begin()->head();
    typename K::value_type step = 0;
    schema::meta_page_t N = 1;

    mdb_view<schema::meta_key_t> meta_k(&schema::META_KEY_PAGE, 1);
    mdb_view<schema::meta_page_t> meta_v;

    rc = parent.get(meta_k, meta_v);
    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
    if(MDB_SUCCESS == rc)
        N = *(meta_v.begin());

    const schema::meta_page_t page_sz = extensions::graphdb::schema::page_size * extensions::bitarray::cellsize;
    const schema::meta_page_t max = N * page_sz - 1;
    assertm(max >= ret, "Out of bounds ", ret, " ", max);

    std::random_device r;
    std::default_random_engine e(r());
    std::uniform_int_distribution<typename K::value_type> d(0, max);

    {
        Cursor cursor(parent.txn_, parent);

        while(true)
        {
            K query = {ret + step, 0};
            if(query.head() <= max && query.head() != schema::RESERVED)
            {
                mdb_view<K> iter_k(&query, 1);
                mdb_view<uint8_t> iter_v;

                rc = cursor.get(iter_k, iter_v, MDB_cursor_op::MDB_SET);
                if(MDB_NOTFOUND == rc) break;
            }
            if(query.head() && !(query.head() % page_sz))
            {
                N += 1;
                step = 0;
                ret = max + 1 + (ret % page_sz);
                break;
            }
            if(ret == hint.begin()->head())
            {
                ret = d(e);
                step = 0;
            }
            else
                step += 1;
        }
    }

    meta_v = mdb_view<schema::meta_page_t>(&N, 1);
    assertm(MDB_SUCCESS == (rc = parent.put(meta_k, meta_v, flags::put::DEFAULT)), rc);

    *(hint.begin()) = {ret + step, 0};
}

template<typename K>
void find(graphdb::Database const& parent, K& hint)
{
    mdb_view<K> hint_v(&hint, 1);
    find(parent, hint_v);
}

}  // namespace head

namespace end
{

template<typename K, typename = schema::is_list_key_t<K>>
void write(graphdb::Database const& parent,
           graphdb::mdb_view<K> node,
           graphdb::mdb_view<schema::list_end_t> end)
{
    assertm(node.begin()->head(), node.begin()->head());
    int rc;
    K iter = *(node.begin());
    mdb_view<K> iter_k(&iter, 1);

    iter.tail() = schema::LIST_TAIL_MAX;
    assertm(MDB_SUCCESS == (rc = parent.put(iter_k, end, flags::put::DEFAULT)), rc, iter);
}

template<typename K>
void write(graphdb::Database const& parent, K node, schema::list_end_t& end)
{
    graphdb::mdb_view<K> node_v(&node, 1);
    graphdb::mdb_view<schema::list_end_t> end_v(&end, 1);
    write(parent, node_v, end_v);
}

template<typename K, typename = schema::is_list_key_t<K>>
void read(graphdb::Database const& parent,
          graphdb::mdb_view<K> node,
          graphdb::mdb_view<schema::list_end_t> end)
{
    assertm(node.begin()->head(), node.begin()->head());
    int rc;
    K iter = *(node.begin());
    mdb_view<K> iter_k(&iter, 1);

    graphdb::Cursor cursor(parent.txn_, parent);

    iter.tail() = schema::LIST_TAIL_MAX;
    mdb_view<schema::list_end_t> iter_v;

    rc = cursor.get(iter_k, iter_v, MDB_cursor_op::MDB_SET);
    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
    if(MDB_SUCCESS == rc)
        memcpy((void*)end.data(), (void*)iter_v.data(), sizeof(schema::list_end_t));
    else
        *(end.begin()) = schema::list_end_t();
}

template<typename K>
void read(graphdb::Database const& parent, K& node, schema::list_end_t& end)
{
    graphdb::mdb_view<K> node_v(&node, 1);
    graphdb::mdb_view<schema::list_end_t> end_v(&end, 1);
    read(parent, node_v, end_v);
}

}  // namespace end

/**
 * Removes a linked list from the DB.
 */
template<typename K, typename = schema::is_list_key_t<K>>
void clear(graphdb::Database const& parent, graphdb::mdb_view<K> node)
{
    assertm(0 != node.begin()->head(), node.begin()->head());

    int rc = 0;

    K iter = *(node.begin());
    mdb_view<K> iter_k(&iter, 1);

    for(size_t i = iter.tail(); i < schema::LIST_TAIL_MAX; i++)
    {
        iter.tail() = i;
        rc = parent.remove(iter_k);
        assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
        if(MDB_NOTFOUND == rc)
            break;
    }

    if(!node.begin()->tail())  // we removed the whole list
    {
        iter.tail() = schema::LIST_TAIL_MAX;
        assertm(MDB_SUCCESS == (rc = parent.remove(iter_k)), rc);
    }
}

template<typename K>
void clear(graphdb::Database const& parent, K& node)
{
    mdb_view<K> node_v(&node, 1);
    clear(parent, node_v);
}

/**
 * Receives the head of the new linked list. Splits the value into page sized
 * chunks and creates the linked list.
 */
template<typename K, typename V, typename = schema::is_list_key_t<K>>
void write(graphdb::Database const& parent, const graphdb::mdb_view<K> head, const graphdb::mdb_view<V> value)
{

    assertm(0 != head.begin()->head(), head.begin()->head());
    assertm(0 == head.begin()->tail(), head.begin()->tail());

    int rc = 0;

    K iter = *(head.begin());
    mdb_view<K> iter_k(&iter, 1);
    mdb_view<V> iter_v;
    size_t chunk = extensions::graphdb::schema::page_size / sizeof(V);  // number of items in a page

    for(typename K::value_type i = 0; i < schema::LIST_TAIL_MAX; i++)
    {
        if(i*chunk >= value.size()) break;
        iter.tail() = i;
        iter_v = mdb_view<V>(value.data()          + i * chunk,
                             std::min(value.size() - i * chunk, chunk));
        assertm(MDB_SUCCESS == (rc = parent.put(iter_k, iter_v, flags::put::DEFAULT)), rc);
    }

    // if we are overwriting an existing list, clear the remainder of the
    // old list
    iter.tail() += 1;
    clear(parent, iter);

    extensions::graphdb::schema::list_end_t end;
    end::read(parent, iter, end);
    assert(0 < end.refcount());
    end.length() = iter.tail();
    end.hash() = std::hash<graphdb::mdb_view<V>>{}(value);
    end.bytes() = value.size() * sizeof(V);
    end::write(parent, iter, end);
}

template<typename K, typename V>
void write(graphdb::Database const& parent, K& head, V& value)
{
    const graphdb::mdb_view<K> head_v(&head, 1);

    if constexpr(std::is_same_v<V, torch::Tensor>)
    {
        assert(value.is_contiguous());
        torch::Tensor tensor = value;
        if(torch::kUInt8 == tensor.dtype())
        {
            mdb_view<uint8_t> value_v(tensor.data_ptr<uint8_t>(), tensor.numel());
            write(parent, head_v, value_v);
        }
        else
            assertm(false, tensor.dtype());
    }
    else if constexpr (extensions::is_contiguous<V>::value)
    {
        const graphdb::mdb_view<typename V::value_type> value_v(value.data(), value.size());
        write(parent, head_v, value_v);
    }
    else if constexpr(std::is_fundamental_v<V>)
    {
        const graphdb::mdb_view<std::remove_cv_t<V>> value_v(&value, 1);
        write(parent, head_v, value_v);
    }
    else
        static_assert(extensions::false_always<K,V>::value);
}

/**
 * Loads the pages of a linked list into the single buffer pointed by the value.
 */
template<typename K, typename V, typename = schema::is_list_key_t<K>>
void read(graphdb::Database const& parent, const graphdb::mdb_view<K> head, graphdb::mdb_view<V> value)
{

    /**
     * If instead you want to read an individual node of a list, call directly the
     * Database.get method.
     */
    assertm(0 != head.begin()->head(), head.begin()->head());
    assertm(0 == head.begin()->tail(), head.begin()->tail());

    int rc = 0;

    {
        graphdb::Cursor cursor(parent.txn_, parent);

        K iter = *(head.begin());
        mdb_view<K> iter_k(&iter, 1);
        mdb_view<V> iter_v;
        size_t chunk = extensions::graphdb::schema::page_size / sizeof(V);

        for(size_t i = 0; i < schema::LIST_TAIL_MAX; i++)
        {
            iter.tail() = i;
            rc = cursor.get(iter_k, iter_v, MDB_cursor_op::MDB_SET);
            if(i == head.begin()->tail())
                assertm(MDB_SUCCESS == rc ,rc);  // node must be in the DB.
            if(MDB_NOTFOUND == rc) break;
            if(iter.head() != head.begin()->head()) break;
            assertm(value.size() >= i * chunk + iter_v.size(), i, sizeof(V), value.size());
            memcpy((void*)(value.data() + i * chunk), (void*)(iter_v.data()), iter_v.size() * sizeof(V));
        }
    }

}

template<typename K, typename V>
void read(graphdb::Database const& parent, K& head, V& value)
{
    const graphdb::mdb_view<K> head_v(&head, 1);

    if constexpr(extensions::is_contiguous<V>::value)
    {
        graphdb::mdb_view<typename V::value_type> value_v(value.data(), value.size());
        read(parent, head_v, value_v);
    }
    else if constexpr(std::is_fundamental_v<V>)
    {
        graphdb::mdb_view<V> value_v(&value, 1);
        read(parent, head_v, value_v);
    }
    else
        static_assert(extensions::false_always<K,V>::value);
}

template<typename K, typename V, typename = schema::is_list_key_t<K>>
bool holds(graphdb::Database const& parent, graphdb::mdb_view<K> head, graphdb::mdb_view<V> value, size_t hash)
{
    assertm(0 != head.begin()->head(), head.begin()->head());
    assertm(0 == head.begin()->tail(), head.begin()->tail());

    bool ret = false;
    int rc = 0;

    K iter = *(head.begin());
    mdb_view<K> iter_k(&iter, 1);

    schema::list_end_t end;
    mdb_view<schema::list_end_t> end_v(&end, 1);

    list::end::read(parent, iter_k, end_v);
    assert(0 < end.refcount());

    // compare the size
    ret = value.size() * sizeof(V) == end.bytes();

    // compare the hash
    ret &= hash == end.hash();

    if(ret)  // compare the contents
    {
        graphdb::Cursor cursor(parent.txn_, parent);

        mdb_view<V> iter_v;
        size_t chunk = extensions::graphdb::schema::page_size / sizeof(V);

        for(size_t i = 0; i < schema::LIST_TAIL_MAX; i++)
        {
            iter.tail() = i;
            rc = cursor.get(iter_k, iter_v, MDB_cursor_op::MDB_SET);
            if(i == head.begin()->tail())
                assertm(MDB_SUCCESS == rc ,rc);  // node must be in the DB.
            if(MDB_NOTFOUND == rc) break;
            if(iter.head() != head.begin()->head()) break;
            ret &= 0 == memcmp((void*)(value.data() + i * chunk), (void*)(iter_v.data()), iter_v.size() * sizeof(V));
        }
    }

    return ret;
}

template<typename K, typename V>
bool holds(graphdb::Database const& parent, K& head, V& value, size_t hash)
{
    bool ret = false;
    const graphdb::mdb_view<K> head_v(&head, 1);

    if constexpr(extensions::is_contiguous<V>::value)
    {
        graphdb::mdb_view<typename V::value_type> value_v(value.data(), value.size());
        ret = holds(parent, head_v, value_v, hash);
    }
    else if constexpr(std::is_fundamental_v<V>)
    {
        graphdb::mdb_view<V> value_v(&value, 1);
        ret = holds(parent, head_v, value_v, hash);
    }
    else
        static_assert(extensions::false_always<K,V>::value);
    return ret;
}

/**
 * Expands, a potentially empty, linked list by a single node.
 */
template<typename K, typename V, typename = schema::is_list_key_t<K>>
void expand(graphdb::Database const& parent, graphdb::mdb_view<K> node, graphdb::mdb_view<V> value)
{
    assertm(0 != node.begin()->head(), node.begin()->head());
    assertm(value.size() * sizeof(V) <= extensions::graphdb::schema::page_size, value.size());

    int rc = 0;

    graphdb::Cursor cursor(parent.txn_, parent);

    K iter = *(node.begin());
    mdb_view<K> iter_k(&iter, 1);
    mdb_view<V> iter_v;

    for(auto i = iter.tail(); i < schema::LIST_TAIL_MAX; ++i)
    {
        iter.tail() = i;
        rc = cursor.get(iter_k, iter_v, MDB_cursor_op::MDB_SET);
        assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
        if(MDB_NOTFOUND == rc || iter_k.begin()->head() != node.begin()->head())
        {
            iter = {node.begin()->head(), i};
            assertm(MDB_SUCCESS == (rc = parent.put(iter_k, value, flags::put::DEFAULT)), rc);
            break;
        }
        if(MDB_SUCCESS == rc)
        {
            // It does not make sense to expand a list with values of variable size.
            // Lists are created due to a) breaking larger buffers into chunks, or
            // b) having a set of identical sized buffers (ie. pages, or keys).
            // This function manages (b).
            assertm(iter_v.size() == value.size(), iter_v.size(), value.size());
        }
    }

    // Update the list's end node.
    extensions::graphdb::schema::list_end_t end;
    end::read(parent, iter, end);  // read to preserve the refcount
    assert(0 < end.refcount());
    end.length() = iter.tail() + 1;
    end.bytes() = (iter.tail() * value.size() + value.size()) * sizeof(V);
    end.hash() = 0;  // we do not set the hash; since we expanded the list,
                     // the list does not hold a single buffer, hence setting
                     // the hash is meaningless
    end::write(parent, iter, end);
}

template<typename K, typename V>
void expand(graphdb::Database const& parent, K& node, V& value)
{
    const graphdb::mdb_view<K> node_v(&node, 1);

    if constexpr(std::is_same_v<V, torch::Tensor>)
    {
        assert(value.is_contiguous());
        torch::Tensor tensor = value;
        if(torch::kUInt8 == tensor.dtype())
        {
            mdb_view<uint8_t> value_v(tensor.data_ptr<uint8_t>(), tensor.numel());
            expand(parent, node_v, value_v);
        }
        else
            assertm(false, tensor.dtype());
    }
    else if constexpr (extensions::is_contiguous<V>::value)
    {
        const graphdb::mdb_view<typename V::value_type> value_v(value.data(), value.size());
        expand(parent, node_v, value_v);
    }
    else
        static_assert(extensions::false_always<K,V>::value);
}

/**
 * Removes one node from a linked list.
 * Shifts the nodes in the range (node, last] of a linked list, to the range
 * [node, last), ie. one step to the left.
 */
template<typename K, typename = schema::is_list_key_t<K>>
void remove(graphdb::Database const& parent, graphdb::mdb_view<K> node)
{
    assertm(0 != node.begin()->head(), node.begin()->head());

    int rc = 0;

    K iter = *(node.begin());
    mdb_view<K> iter_k(&iter, 1);
    mdb_view<uint8_t> iter_v;
    size_t sz = 0;

    for(size_t i = iter.tail(); i < schema::LIST_TAIL_MAX; i++)
    {
        iter.tail() = i;
        rc = parent.get(iter_k, iter_v);
        if(iter.tail() == node.begin()->tail())
        {
            assertm(MDB_SUCCESS == rc ,rc);  // key must be in the DB.

            /**
             * Linked lists are created due to (a) breaking larger buffers into
             * chunks, or (b) managing sets of values with identical sizes. It
             * does not make sense to remove a node and shift the rest in the
             * case of (a). This function manages (b).
             */

            sz = iter_v.size();
            goto DELETE;
        }
        else if(MDB_NOTFOUND == rc)
            break;
        iter.tail() = i-1;  // replace the previous node
        assertm(MDB_SUCCESS == (rc = parent.put(iter_k, iter_v, flags::put::DEFAULT)), rc);
DELETE:
        iter.tail() = i;  // delete the current node
        assertm(MDB_SUCCESS == (rc = parent.remove(iter_k)), rc);
    }
    if(sz)  // ie. we removed an element
    {
        iter.tail() -= 2;  // point to the last node

        extensions::graphdb::schema::list_end_t end;
        end::read(parent, iter, end);  // read to preserve the refcount
        assert(0 < end.refcount());
        end.length() = iter.tail() + 1;
        end.bytes() = iter.tail() * sz + sz;
        end.hash() = 0;  // we do not set the hash; since we expanded the list,
                         // the list does not hold a single buffer, hence setting
                         // the hash is meaningless
        end::write(parent, iter, end);
    }
}

template<typename K>
void remove(graphdb::Database const& parent, K& node)
{
    graphdb::mdb_view<K> node_v(&node, 1);
    remove(parent, node_v);
}

}  // namespace list

namespace refcount
{

/**
 * Entries of the main table have the form of <key,head> pairs. The functions in
 * this namespace manage these entries, plus the reference counting. When a new
 * list enters the DB, the functions of these namespace are responsible for
 * creating the end nodes and set the reference counting to 1. Every other
 * function that needs to access an end node asserts that the end node is
 * present. This way we can verify that a) the right APIs are used, and b) the
 * integrity of the DB. When the refcount drops to zero, the list is removed,
 * otherwise only the <key,head> pair is removed.
 */

template<typename K, typename V, typename = schema::is_key_t<K>, typename = schema::is_list_key_t<V>>
void increase(schema::DatabaseSet const& parent, graphdb::mdb_view<K> key, graphdb::mdb_view<V> head)
{

    assertm(0 == head.begin()->tail(), head.begin()->tail());
    assertm(0 != head.begin()->head(), head.begin()->head());
    assertm(schema::LIST_TAIL_MAX != head.begin()->head(), head.begin()->head());

    int rc = 0;

    schema::list_key_t iter = {head.begin()->head(), schema::LIST_TAIL_MAX};
    schema::list_end_t end;

    list::end::read(parent.list_, iter, end);
    end.refcount() += 1;
    list::end::write(parent.list_, iter, end);

    assertm(MDB_SUCCESS == (rc = parent.main_.put(key, head, extensions::graphdb::flags::put::DEFAULT)), rc);

}

template<typename K, typename V>
void increase(schema::DatabaseSet const& parent, K key, V head)
{
    graphdb::mdb_view<K> key_v(&key, 1);
    graphdb::mdb_view<V> head_v(&head, 1);
    increase(parent, key_v, head_v);
}

template<typename K, typename V, typename = schema::is_key_t<K>, typename = schema::is_list_key_t<V>>
void decrease(schema::DatabaseSet const& parent, graphdb::mdb_view<K> key, graphdb::mdb_view<V> head)
{
    int rc = 0;

    schema::list_end_t end;

    graphdb::mdb_view<schema::list_key_t> iter_v;

    assertm(MDB_SUCCESS == (rc = parent.main_.get(key, iter_v)), rc);
    *(head.begin()) = {iter_v.begin()->head(), schema::LIST_TAIL_MAX};

    list::end::read(parent.list_, *(head.begin()), end);
    assert(0 < end.refcount());
    end.refcount() -= 1;

    if(end.refcount())
    {
        list::end::write(parent.list_, *(head.begin()), end);
        *(head.begin()) = {0,0};  // the list has not been removed, the head
                                  // should not be reused
    }
    else
    {
        head.begin()->tail() = 0;
        list::clear(parent.list_, *(head.begin()));

        // the list will be removed, the head can be reused so leave it
        // pointing to the old list
        *(head.begin()) = {head.begin()->head(), 0};
    }

    assertm(MDB_SUCCESS == (rc = parent.main_.remove(key)), rc);
}

template<typename K, typename V>
void decrease(schema::DatabaseSet const& parent, K const& key, V& head)
{
    graphdb::mdb_view<K> key_v(&key, 1);
    graphdb::mdb_view<V> head_v(&head, 1);
    decrease(parent, key_v, head_v);
}

}  // namespace refcount

namespace hash  // Transactions that manage the hash DBs.
{

/**
 * We hash the values through mdb_view, as python needs to pass py::bytes,
 * so we need to have a uniform hash value for all types.
 */
template<typename V>
size_t make(V const& data)
{
    size_t ret = 0;
    if constexpr(std::is_same_v<V, torch::Tensor>)
    {
        assert(data.is_contiguous());
        torch::Tensor tensor = data;
        if(torch::kFloat == tensor.dtype())
        {
            mdb_view<float> data_v(tensor.data_ptr<float>(), tensor.numel());
            ret = std::hash<mdb_view<float>>{}(data_v);
        }
        else if(torch::kUInt8 == tensor.dtype())
        {
            mdb_view<uint8_t> data_v(tensor.data_ptr<uint8_t>(), tensor.numel());
            ret = std::hash<mdb_view<uint8_t>>{}(data_v);
        }
        else
            assertm(false, tensor.dtype());
    }
    else if constexpr(extensions::is_contiguous<V>::value)
    {
        mdb_view<typename V::value_type> data_v(data.data(), data.size());
        ret = std::hash<mdb_view<typename V::value_type>>{}(data_v);
    }
    else if constexpr(std::is_fundamental_v<V>)
    {
        mdb_view<V> data_v(&data, 1);
        ret = std::hash<mdb_view<V>>{}(data_v);
    }
    else
        static_assert(extensions::false_always<V>::value);
    return ret;
}

template<typename K, typename V, typename = schema::is_list_key_t<K>, typename = schema::is_key_t<V>>
void write(graphdb::Database const& parent, graphdb::mdb_view<K> hash, graphdb::mdb_view<V> key)
{
    assertm(0 != hash.begin()->head(), hash.begin()->head());
    assertm(0 == hash.begin()->tail(), hash.begin()->tail());

    schema::list_key_t iter = {hash.begin()->head(), schema::LIST_TAIL_MAX};
    schema::list_end_t end;

    list::end::read(parent, iter, end);
    if(!end.refcount())
    {
        end.refcount() = 1;
        list::end::write(parent, iter, end);
    }
    // Nothing should hold a reference to a list of the hash table.
    assertm(1 == end.refcount(), end.refcount());

    list::expand(parent, hash, key);
}

template<typename K, typename V>
void write(graphdb::Database const& parent, K& hash, V& key)
{
    graphdb::mdb_view<K> hash_v(&hash, 1);
    graphdb::mdb_view<V> key_v(&key, 1);
    write(parent, hash_v, key_v);
}

template<typename K, typename = schema::is_key_t<K>>
using visitor_t = std::function<int(schema::list_key_t, K)>;

template<typename V, typename K, schema::is_list_key_t<V>* = nullptr, schema::is_key_t<K>* = nullptr>
int visit(graphdb::Database const& parent, V& hash, visitor_t<K> visitor)
{
    int rc = MDB_SUCCESS;

    K key;

    for(size_t i = hash.tail(); i < schema::LIST_TAIL_MAX; i++)
    {
        hash.tail() = i;
        rc = parent.get(hash, key);
        assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, key);
        if(MDB_SUCCESS != rc) break;
        rc = visitor(hash, key);
        if(MDB_SUCCESS == rc) break;
    }

    return rc;
}

template<typename V, typename K, schema::is_not_list_key_t<V>* = nullptr, schema::is_key_t<K>* = nullptr>
int visit(graphdb::Database const& parent, V& value, visitor_t<K> visitor)
{
    graphdb::schema::list_key_t hash = {extensions::graphdb::hash::make(value), 0};

    return visit(parent, hash, visitor);
}


}  // namespace hash

namespace feature
{

/**
 * A feature is read from the DB in two steps A) lookup, which gives us the
 * head of the list and the size of the buffer, B) using the head of the list
 * and the size of the buffer, we read the data from the DB. It also passes the
 * hash value of the held buffer to the visitor.
 */

using visitor_t = std::function<int(schema::list_key_t, size_t /*bytes*/, size_t /*hash*/)>;

template <typename K>
int visit(schema::DatabaseSet const& parent, K& key, visitor_t visitor)
{
    int rc = 0;
    schema::list_key_t iter;
    size_t size = 0;
    rc = parent.main_.get(key, iter);
    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, key);
    if(MDB_SUCCESS == rc)
    {
        schema::list_end_t end;
        extensions::graphdb::list::end::read(parent.list_, iter, end);
        assert(0 < end.refcount());
        rc = visitor(iter, end.bytes(), end.hash());
    }
    return rc;
}

template <typename K, typename V>
void write(schema::DatabaseSet const& parent, K const& key, V& value, bool hashed)
{
    int rc = 0;
    schema::list_key_t head = {0,0};
    schema::list_key_t hash = {extensions::graphdb::hash::make(value), 0};
    schema::list_end_t end;

    rc = parent.main_.get(key, head);
    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, key);

    if(MDB_SUCCESS == rc)
    {
        list::end::read(parent.list_, head, end);
        bool present = list::holds(parent.list_, head, value, hash.head());
        if(present) return;  // noop
        else
            extensions::graphdb::refcount::decrease(parent, key, head);
    }

    // See if the value is already in the DB.
    extensions::graphdb::hash::visitor_t<K> visitor =
            [&](schema::list_key_t h4sh, K k3y)
            {
                assertm(hash.head() == h4sh.head(), hash.head(), h4sh.head());
                return visit(parent, k3y,
                             [&](schema::list_key_t iter, size_t size, size_t hAsh) -> int
                             {
                                 bool stored = list::holds(parent.list_, iter, value, hash.head());
                                 if(stored) head = iter;
                                 return  stored ? MDB_SUCCESS : MDB_NOTFOUND;
                             });
            };
    hash::visit(parent.hash_, value, visitor);

    if(!head.head())  // The value is not in the DB.
        extensions::graphdb::list::head::find(parent.list_, head);

    extensions::graphdb::refcount::increase(parent, key, head);

    if(hashed)  // when hashed, do not write duplicates
    {
        // remove the old hash/key pair
        schema::list_key_t old = {end.hash(), 0};

        extensions::graphdb::hash::visitor_t<K> visitor =
                [&](schema::list_key_t h4sh, K k3y)
                {
                    if(key == k3y)
                    {
                        list::remove(parent.hash_, h4sh);
                        return MDB_SUCCESS;
                    }
                    return MDB_NOTFOUND;
                };
        hash::visit(parent.hash_, old, visitor);

        // add the new hash/key pair
        hash::write(parent.hash_, hash, key);
    }

    extensions::graphdb::list::write(parent.list_, head, value);
}

}  // namespace feature

namespace bitarray  // Transactions that manage the bitmaps.
{
    static std::tuple<schema::list_key_t::value_type, schema::list_key_t::value_type, schema::list_key_t::value_type>
    map_vtx_to_page(schema::graph_vtx_set_key_t::value_type vtx)
    {
        schema::graph_vtx_set_key_t::value_type slice = extensions::graphdb::schema::page_size * extensions::bitarray::cellsize;
        schema::graph_vtx_set_key_t::value_type tail = vtx / slice;  // ie. the page in the list of pages
        schema::graph_vtx_set_key_t::value_type map  = vtx % slice;  // ie. the index of the vertex in the page
        schema::graph_vtx_set_key_t::value_type cell = map / extensions::bitarray::cellsize;  // ie. the cell in the page

        return {tail, cell, map};
    }

namespace page
{

    /**
     * Updates the iter with the head of the list. If the cached key and iter
     * point to the same page becomes a noop. If the key and iter point to a
     * non-existent page, it returns an empty page. The page is loaded into the
     * passed tensor.
     */
    template<typename K, typename = schema::is_key_t<K>>
    static void load(schema::DatabaseSet const& parent,
                     K key,
                     schema::list_key_t& iter,
                     K& key_cached,
                     schema::list_key_t& iter_cached,
                     torch::Tensor page)
    {
        int rc = MDB_SUCCESS;

        if(key == key_cached && iter == iter_cached && 0 != iter.head() && schema::LIST_TAIL_MAX != iter.head())
            return;  // ie. the page has been loaded

        if(key != key_cached || iter != iter_cached || 0 == iter.head() || schema::LIST_TAIL_MAX == iter.head())
        {
            schema::list_key_t head = {0,0};
            rc = parent.main_.get(key, head);  // get head of list
            assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, key);
            if(MDB_SUCCESS == rc)
            {
                iter.head() = head.head();
                rc = parent.list_.get(iter, page);
                assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
            }
        }

        /**
         * A NOTFOUND here means that the key or the iter were not in the DB.
         */

        if(MDB_NOTFOUND == rc && key == key_cached && iter == iter_cached && schema::LIST_TAIL_MAX == iter.head())
        {
            rc = MDB_SUCCESS;  // ie. noop, an empty page has already been returned
        }
        else if(MDB_NOTFOUND == rc)
        {
            page.zero_();
            iter.head() = schema::LIST_TAIL_MAX;
            rc = MDB_SUCCESS;
        }

        if(MDB_SUCCESS == rc)
        {
            key_cached = key;
            iter_cached = iter;
        }
    }

    template<typename K, typename = schema::is_key_t<K>>
    static void write(schema::DatabaseSet const& vtx_set,
                      schema::DatabaseSet const& parent,
                      K key,
                      schema::list_key_t& iter,
                      torch::Tensor page)
    {
        int rc = 0;

        schema::list_key_t head = {0,0};
        rc = parent.main_.get(key, head);  // get head of list
        assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, key);
        if(MDB_NOTFOUND == rc)
        {
            extensions::graphdb::list::head::find(parent.list_, head);
            extensions::graphdb::refcount::increase(parent, key, head);

           if(iter.tail())  // reserve the head of the list
           {
               torch::Tensor blob = torch::zeros(page.sizes(), page.options());
               assertm(MDB_SUCCESS == (rc = parent.list_.put(head, blob, extensions::graphdb::flags::put::DEFAULT)), rc, head);
           }
        }

        iter.head() = head.head();
        assertm(MDB_SUCCESS == (rc = parent.list_.put(iter, page, extensions::graphdb::flags::put::DEFAULT)), rc, iter);

        {
            /**
             * The vertex set might have been expanded since the last time we
             * wrote to the adj mtx. Update the end node of this list to follow
             * the size of the vtx set.
             */

            schema::graph_vtx_set_key_t vtx_set_key = {key.graph()};
            graphdb::schema::list_key_t end_i;
            graphdb::schema::list_end_t end;

            // get the end node of the vtx set
            assertm(MDB_SUCCESS == (rc = vtx_set.main_.get(vtx_set_key, end_i)), rc);
            extensions::graphdb::list::end::read(vtx_set.list_, end_i, end);

            // There should only be one reference to the vtx set, and similarly
            // to each row of the adj mtx.
            assertm(1 == end.refcount(), end.refcount());

            // copy the end node to the adj mtx list
            end_i = {head.head(), 0};  // switch to the adj mtx list
            extensions::graphdb::list::end::write(parent.list_, end_i, end);
        }
    }

}  // namespace page
}  // namespace bitarray

namespace stat
{

}  // namespace stat

}}  // namespace extensions::graphdb

#endif  // EXTENSIONS_GRAPHDB_GRAPHDB_H
