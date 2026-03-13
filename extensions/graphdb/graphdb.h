#ifndef EXTENSIONS_GRAPHDB_GRAPHDB_H
#define EXTENSIONS_GRAPHDB_GRAPHDB_H

namespace extensions { namespace graph { struct Graph; }}

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
 *   and returns N*PAGE_SIZE*CHAR_BIT.
 */
template<typename K, typename = schema::is_list_key_t<K>>
void find(graphdb::Database const& parent, graphdb::mdb_view<K> hint)
{
    assertm(0 == hint.begin()->tail(), hint.begin()->tail());

    int rc = 0;

    typename K::value_type ret = hint.begin()->head();
    typename K::value_type step = 0;
    schema::list_hdr_t hdr;

    mdb_view<schema::meta_key_t> meta_k(&schema::META_LIST_HEAD, 1);
    mdb_view<schema::list_hdr_t> meta_v;

    rc = parent.get(meta_k, meta_v);
    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
    if(MDB_SUCCESS == rc)
        hdr = *(meta_v.begin());
    else
        hdr.length() = 1;

    const schema::list_hdr_t::value_type page_sz = extensions::graphdb::schema::page_size * extensions::bitarray::cellsize;
    const schema::list_hdr_t::value_type max = hdr.length() * page_sz - 1;
    assertm(max >= ret, "Out of bounds ", ret, " ", max);

    std::random_device r;
    std::default_random_engine e(r());
    std::uniform_int_distribution<typename K::value_type> d(0, max);

    {
        Cursor cursor(parent.txn_, parent);

        while(true)
        {
            // Every list has a header node, and an empty list has only a header node.
            K query = {ret, 0};
            if(query.head() <= max && query.head() != schema::RESERVED)
            {
                mdb_view<K> iter_k(&query, 1);
                mdb_view<uint8_t> iter_v;

                rc = cursor.get(iter_k, iter_v, MDB_cursor_op::MDB_SET);
                if(MDB_NOTFOUND == rc) break;
            }
            if(step >= 0.5 * page_sz)
            {
                hdr.length() += 1;
                ret = max + 1;
                break;
            }
            else
            {
                ret = d(e);
                step += 1;
            }
        }
    }

    meta_v = mdb_view<schema::list_hdr_t>(&hdr, 1);
    assertm(MDB_SUCCESS == (rc = parent.put(meta_k, meta_v, flags::put::DEFAULT)), rc);

    *(hint.begin()) = {ret, 0};
}

template<typename K>
void find(graphdb::Database const& parent, K& hint)
{
    mdb_view<K> hint_v(&hint, 1);
    find(parent, hint_v);
}

}  // namespace head

/**
 * Removes a linked list from the DB.
 */
template<typename K, typename = schema::is_list_key_t<K>>
void purge(graphdb::Database const& parent, graphdb::mdb_view<K> node)
{
    assertm(0 != node.begin()->head(), node.begin()->head());

    int rc = 0;

    K iter = *(node.begin());
    mdb_view<K> iter_k(&iter, 1);

    for(size_t i = iter.tail();; i++)
    {
        iter.tail() = i;
        if(!i)
        {
            extensions::graphdb::schema::list_hdr_t hdr;
            assertm(MDB_SUCCESS == (rc = parent.get(iter, hdr)), rc);
            assertm(1 == hdr.refcount(), iter, hdr);  // ie. do not purge a list that has more than one references
        }
        rc = parent.remove(iter_k);
        assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
        if(MDB_NOTFOUND == rc)
            break;
    }

    // NB. it does not update the header!
}

template<typename K>
void purge(graphdb::Database const& parent, K& node)
{
    mdb_view<K> node_v(&node, 1);
    purge(parent, node_v);
}

/**
 * Receives the head of the new linked list. Splits the value into page sized
 * chunks and creates the linked list.
 */
template<typename K, typename V, typename = schema::is_list_key_t<K>>
void write(graphdb::Database const& parent, const graphdb::mdb_view<K> head, const graphdb::mdb_view<V> value, typename K::value_type hash)
{

    assertm(0 != head.begin()->head(), head.begin()->head());
    assertm(0 == head.begin()->tail(), head.begin()->tail());
    assert(0 < value.size());

    int rc = 0;

    K iter = *(head.begin());
    mdb_view<K> iter_k(&iter, 1);
    mdb_view<V> iter_v;
    size_t chunk = extensions::graphdb::schema::page_size / sizeof(V);  // number of items in a page

    extensions::graphdb::schema::list_hdr_t hdr;
    mdb_view<extensions::graphdb::schema::list_hdr_t> hdr_v;

    for(typename K::value_type i = 0;; i++)
    {
        if(!i)
        {
            rc = parent.get(iter_k, hdr_v);
            assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
            if(MDB_SUCCESS == rc)
            {
                hdr = *(hdr_v.begin());
                assertm(1 == hdr.refcount(), iter, hdr);  // ie. do not overwrite a list that has more than one references
            }
            // NB. it does not change the ref count!
            hdr.length() = value.size() / chunk;
            hdr.length() += value.size() % chunk ? 1 : 0;
            hdr.bytes() = value.size() * sizeof(V);
            hdr.hash() = hash;
            hdr_v = mdb_view<schema::list_hdr_t>(&hdr, 1);
            assertm(MDB_SUCCESS == (rc = parent.put(iter_k, hdr_v, flags::put::DEFAULT)), rc);
        }
        else
        {
            if((i-1)*chunk >= value.size()) break;
            iter.tail() = i;
            iter_v = mdb_view<V>(value.data()          + (i-1) * chunk,
                                 std::min(value.size() - (i-1) * chunk, chunk));
            assertm(MDB_SUCCESS == (rc = parent.put(iter_k, iter_v, flags::put::DEFAULT)), rc);
        }
    }

    // if we are overwriting an existing list, purge the remainder of the
    // old list
    iter.tail() += 1;
    purge(parent, iter);
}

template<typename K, typename V>
void write(graphdb::Database const& parent, K& head, V& value, typename K::value_type hash)
{
    const graphdb::mdb_view<K> head_v(&head, 1);

    if constexpr(std::is_same_v<V, torch::Tensor>)
    {
        assert(value.is_contiguous());
        torch::Tensor tensor = value;
        if(torch::kUInt8 == tensor.dtype())
        {
            mdb_view<uint8_t> value_v(tensor.data_ptr<uint8_t>(), tensor.numel());
            write(parent, head_v, value_v, hash);
        }
        else
            assertm(false, tensor.dtype());
    }
    else if constexpr (extensions::is_contiguous<V>::value)
    {
        const graphdb::mdb_view<typename V::value_type> value_v(value.data(), value.size());
        write(parent, head_v, value_v, hash);
    }
    else if constexpr(std::is_fundamental_v<V>)
    {
        const graphdb::mdb_view<std::remove_cv_t<V>> value_v(&value, 1);
        write(parent, head_v, value_v, hash);
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
    assertm(0 != head.begin()->head(), *(head.begin()));
    assertm(0 == head.begin()->tail(), *(head.begin()));

    int rc = 0;

    {
        graphdb::Cursor cursor(parent.txn_, parent);

        K iter = *(head.begin());
        mdb_view<K> iter_k(&iter, 1);
        mdb_view<V> iter_v;
        size_t chunk = extensions::graphdb::schema::page_size / sizeof(V);
        extensions::graphdb::schema::list_hdr_t hdr;

        for(size_t i = 0;; i++)
        {
            iter.tail() = i;
            if(!i)
            {
                rc = cursor.get(iter, hdr, MDB_cursor_op::MDB_SET);
                assertm(MDB_SUCCESS == rc ,rc);  // the list must be in the DB.
                assertm(value.size() * sizeof(V) == hdr.bytes(), hdr);
                assert(0 < hdr.refcount());
            }
            else
            {
                rc = cursor.get(iter_k, iter_v, MDB_cursor_op::MDB_NEXT);
                assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
                if(MDB_NOTFOUND == rc) break;
                assertm(value.size() >= (i-1) * chunk + iter_v.size(), i, sizeof(V), value.size());
                memcpy((void*)(value.data() + (i-1) * chunk), (void*)(iter_v.data()), iter_v.size() * sizeof(V));
            }
        }
    }
}

template<typename K, typename V>
void read(graphdb::Database const& parent, K& head, V& value)
{
    const graphdb::mdb_view<K> head_v(&head, 1);

    if constexpr(std::is_same_v<V, torch::Tensor>)
    {
        assert(value.is_contiguous());
        torch::Tensor tensor = value;
        if(torch::kUInt8 == tensor.dtype())
        {
            mdb_view<uint8_t> value_v(tensor.data_ptr<uint8_t>(), tensor.numel());
            read(parent, head_v, value_v);
        }
        else
            assertm(false, tensor.dtype());
    }
    else if constexpr(extensions::is_contiguous<V>::value)
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
    mdb_view<uint8_t> iter_v;

    schema::list_hdr_t hdr;
    mdb_view<schema::list_hdr_t> hdr_v;

    size_t chunk = extensions::graphdb::schema::page_size / sizeof(V);

    graphdb::Cursor cursor(parent.txn_, parent);

    for(size_t i = 0;; i++)
    {
        iter.tail() = i;
        if(!i)
        {
            rc = cursor.get(iter_k, hdr_v, MDB_cursor_op::MDB_SET);
            assertm(MDB_SUCCESS == rc ,rc);  // the list must be in the DB.
            hdr = *(hdr_v.begin());
            assert(0 < hdr.refcount());

            // compare the size
            ret = value.size() * sizeof(V) == hdr.bytes();
            // compare the hash
            ret &= hash == hdr.hash();
        }
        else
        {
            rc = cursor.get(iter_k, iter_v, MDB_cursor_op::MDB_NEXT);
            
            assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
            if(MDB_NOTFOUND == rc) break;

            ret &= 0 == memcmp((void*)(value.data() + (i-1) * chunk), iter_v.data(), iter_v.size());
        }

        if(!ret) break;
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
    assertm(0 != node.begin()->head(), *(node.begin()));
    assertm(value.size() * sizeof(V) <= extensions::graphdb::schema::page_size, value.size());

    int rc = 0;

    K iter = *(node.begin());
    mdb_view<K> iter_k(&iter, 1);
    mdb_view<V> iter_v;

    schema::list_hdr_t hdr;
    mdb_view<schema::list_hdr_t> hdr_v;

    graphdb::Cursor cursor(parent.txn_, parent);

    for(size_t i = 0;; i = hdr.length())  // visit hdr and jump to the index of the newly appended node
    {
        iter.tail() = i;
        if(!i)
        {
            rc = cursor.get(iter_k, hdr_v, MDB_cursor_op::MDB_SET);
            assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
            if(MDB_SUCCESS == rc)
            {
                hdr = *(hdr_v.begin());
                assert(0 < hdr.refcount());
            }
            hdr.length() += 1;
            hdr.bytes() += value.size() * sizeof(V);
            // It does not make sense to expand a list with values of variable size.
            // Lists are created due to a) breaking larger buffers into chunks, or
            // b) having a set of identical sized buffers (ie. pages, or keys).
            // This function manages (b).
            assertm(0 == hdr.bytes() % (value.size() * sizeof(V)), hdr, value.size(), sizeof(V));
            // we do not set the hash; either its a list of the hash table in which case
            // the hash of the header is the same with the hash of the head of the list, or we
            // expanded a list that does not hold a single buffer and the hash is meaningless
            hdr_v = mdb_view<schema::list_hdr_t>(&hdr, 1);
            assertm(MDB_SUCCESS == (rc = parent.put(iter_k, hdr_v, flags::put::DEFAULT)), rc);
        }
        else
        {
            rc = cursor.get(iter_k, iter_v, MDB_cursor_op::MDB_SET);
            assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
            if(MDB_NOTFOUND == rc)
            {
                // since the nodes are in the range [1,n], the hdr length should be equal
                // to the index of the appended node
                assertm(iter.tail() == hdr.length(), iter, hdr);
                assertm(MDB_SUCCESS == (rc = parent.put(iter_k, value, flags::put::DEFAULT)), rc);
                break;
            }
        }
    }
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
    assertm(0 != node.begin()->head(), *(node.begin()));
    assertm(0 <  node.begin()->tail(), *(node.begin()));

    int rc = 0;

    K iter = *(node.begin());
    mdb_view<K> iter_k(&iter, 1);
    mdb_view<uint8_t> iter_v;
    size_t sz = 0;

    extensions::graphdb::schema::list_hdr_t hdr;
    mdb_view<extensions::graphdb::schema::list_hdr_t> hdr_v;

    for(size_t i = iter.tail();; i++)
    {
        iter.tail() = i;
        rc = parent.get(iter_k, iter_v);
        if(iter.tail() == node.begin()->tail())
        {
            assertm(MDB_SUCCESS == rc ,rc, iter);  // key must be in the DB.

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
        iter.tail() = 0;
        assertm(MDB_SUCCESS == (rc = parent.get(iter_k, hdr_v)), rc);
        hdr = *(hdr_v.begin());
        assertm(1 == hdr.refcount(), iter, hdr);  // ie. do not modify a list that has more than one references
        assertm(0 != hdr.hash(), iter, hdr);  // ie. do not modify the hash of the header
        hdr.length() -= 1;
        hdr.bytes() -= sz;
        hdr_v = mdb_view<schema::list_hdr_t>(&hdr, 1);
        assertm(MDB_SUCCESS == (rc = parent.put(iter_k, hdr_v, flags::put::DEFAULT)), rc);
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
 * creating the header nodes and set the reference counting to 1. Every other
 * function that needs to access a header node asserts that the node is
 * present. This way we can verify that a) the right APIs are used, and b) the
 * integrity of the DB. When the refcount drops to zero, the list is removed,
 * otherwise only the <key,head> pair is removed.
 */

template<typename K, typename V, typename = schema::is_key_t<K>, typename = schema::is_list_key_t<V>>
void increase(schema::DatabaseSet const& parent, graphdb::mdb_view<K> key, graphdb::mdb_view<V> head)
{

    assertm(0 == head.begin()->tail(), head.begin()->tail());
    assertm(0 != head.begin()->head(), head.begin()->head());

    int rc = 0;

    schema::list_key_t iter = {head.begin()->head(), 0};
    schema::list_hdr_t hdr;

    rc = parent.list_.get(iter, hdr);
    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
    hdr.refcount() += 1;
    assertm(MDB_SUCCESS == (rc = parent.list_.put(iter, hdr, flags::put::DEFAULT)), rc);

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

    schema::list_hdr_t hdr;
    graphdb::mdb_view<schema::list_hdr_t> hdr_v;

    graphdb::mdb_view<schema::list_key_t> iter_v;

    assertm(MDB_SUCCESS == (rc = parent.main_.get(key, iter_v)), rc);
    *(head.begin()) = *(iter_v.begin());

    assertm(MDB_SUCCESS == (rc = parent.list_.get(head, hdr_v)), rc);
    hdr = *(hdr_v.begin());
    
    assert(0 < hdr.refcount());
    hdr.refcount() -= 1;

    if(hdr.refcount())
    {
        hdr_v = mdb_view<schema::list_hdr_t>(&hdr, 1);
        assertm(MDB_SUCCESS == (rc = parent.list_.put(head, hdr_v, flags::put::DEFAULT)), rc);

        // the list has not been removed, signal the caller
        // that they need to find a new head
        *(head.begin()) = {schema::RESERVED, 0};
    }
    else
    {
        head.begin()->tail() = 0;
        list::purge(parent.list_, *(head.begin()));

        // the list has been removed, the caller can reuse the head
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
    assert(ret);  // no hash value should be zero
    return ret;
}

template<typename K, typename V, typename = schema::is_list_key_t<K>, typename = schema::is_key_t<V>>
void write(graphdb::Database const& parent, graphdb::mdb_view<K> hash, graphdb::mdb_view<V> key)
{
    assertm(0 != hash.begin()->head(), hash.begin()->head());
    assertm(0 == hash.begin()->tail(), hash.begin()->tail());

    int rc = 0;

    schema::list_key_t iter = {hash.begin()->head(), 0};
    schema::list_hdr_t hdr;

    rc = parent.get(iter, hdr);
    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
    if(MDB_NOTFOUND == rc)
    {
        hdr.hash() = iter.head();
        hdr.refcount() = 1;
        assertm(MDB_SUCCESS == (rc = parent.put(iter, hdr, flags::put::DEFAULT)), rc);
    }

    // Nothing should hold a reference to a list of the hash table.
    assertm(1 == hdr.refcount(), hdr);
    assertm(hdr.hash() == iter.head(), hdr, iter);

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

template<typename K, schema::is_key_t<K>* = nullptr>
int visit(graphdb::Database const& parent, graphdb::schema::list_key_t hash, visitor_t<K> visitor)
{
    assertm(0 != hash.head(), hash);

    int rc = MDB_SUCCESS;

    K key;
    schema::list_key_t iter = hash;
    schema::list_hdr_t hdr;

    graphdb::Cursor cursor(parent.txn_, parent);

    for(size_t i = 0;; i++)
    {
        iter.tail() = i;
        if(!i)
        {
            rc = cursor.get(iter, hdr, MDB_cursor_op::MDB_SET);
            assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, hash, iter);
            if(MDB_NOTFOUND == rc) break;
            assertm(hdr.hash() == hash.head(), hdr, hash);
            assertm(0 < hdr.length(), hdr, hash);
            assertm(1 == hdr.refcount(), hdr, hash);
        }
        else
        {
            rc = cursor.get(iter, key, MDB_cursor_op::MDB_NEXT);
            assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, hash, iter);
            if(MDB_NOTFOUND == rc) break;
            rc = visitor(iter, key);
            if(MDB_SUCCESS == rc) break;
        }
    }

    return rc;
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
    rc = parent.main_.get(key, iter);
    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, key);
    if(MDB_SUCCESS == rc)
    {
        schema::list_hdr_t hdr;
        assertm(MDB_SUCCESS == (rc = parent.list_.get(iter, hdr)), rc);
        assert(0 < hdr.refcount());
        rc = visitor(iter, hdr.bytes(), hdr.hash());
    }
    return rc;
}

template <typename K, typename V>
void write(schema::DatabaseSet const& parent, K key, V& value)
{
    bool present = false;
    int rc = 0;

    schema::list_key_t head = {schema::RESERVED, 0};
    schema::list_key_t hash = {extensions::graphdb::hash::make(value), 0};
    schema::list_hdr_t hdr;

    rc = parent.main_.get(key, head);
    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, key);

    if(MDB_SUCCESS == rc)
    {
        assertm(MDB_SUCCESS == (rc = parent.list_.get(head, hdr)), rc);
        // Every value is hashed and refcounted.
        assertm((hdr.hash() && 1 <= hdr.refcount()), hdr);
        present = list::holds(parent.list_, head, value, hash.head());
        if(present) return;  // noop
        else
        {
            extensions::graphdb::refcount::decrease(parent, key, head);
            head = {schema::RESERVED, 0};
        }
    }

    // See if the value is already in the DB.
    extensions::graphdb::hash::visitor_t<K> visitor =
            [&](schema::list_key_t h4sh, K k3y)
            {
                assertm(hash.head() == h4sh.head(), hash.head(), h4sh.head());
                return visit(parent, k3y,
                             [&](schema::list_key_t iter, size_t size, size_t hAsh) -> int
                             {
                                 present = list::holds(parent.list_, iter, value, hash.head());
                                 if(present) head = iter;
                                 return  present ? MDB_SUCCESS : MDB_NOTFOUND;
                             });
            };
    hash::visit(parent.hash_, hash, visitor);

    if(!head.head())  // The value is not in the DB.
    {
        extensions::graphdb::list::head::find(parent.list_, head);
        extensions::graphdb::list::write(parent.list_, head, value, hash.head());
    }

    extensions::graphdb::refcount::increase(parent, key, head);

    {
        // remove the old hash/key pair
        schema::list_key_t old = {hdr.hash(), 0};

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
        if(hdr.hash())
            hash::visit(parent.hash_, old, visitor);            

        // add the new hash/key pair
        hash::write(parent.hash_, hash, key);
    }
}

template <typename K>
void purge(schema::DatabaseSet const& parent, K const& key)
{
    int rc = 0;
    schema::list_key_t iter;
    schema::list_key_t hash;
    schema::list_hdr_t hdr;

    rc = parent.main_.get(key, iter);
    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, key);
    if(MDB_NOTFOUND == rc) return;

    assertm(MDB_SUCCESS == (rc = parent.list_.get(iter, hdr)), rc);
    hash = {hdr.hash(), 0};

    // remove the key from the hash
    extensions::graphdb::hash::visitor_t<K> visitor =
            [&](schema::list_key_t h4sh, K k3y)
            {
                assertm(hash.head() == h4sh.head(), hash.head(), h4sh.head());
                if(key == k3y)
                {
                    list::remove(parent.hash_, h4sh);
                    return MDB_SUCCESS;
                }
                return MDB_NOTFOUND;
            };
    hash::visit(parent.hash_, hash, visitor);

    // decrease the refcount, remove the key from the main table
    extensions::graphdb::refcount::decrease(parent, key, iter);
}

}  // namespace feature

namespace bitarray  // Transactions that manage the bitmaps.
{
    static std::tuple<schema::list_key_t::value_type, schema::list_key_t::value_type, schema::list_key_t::value_type>
    map_vtx_to_page(schema::graph_vtx_set_key_t::value_type vtx)
    {
        schema::graph_vtx_set_key_t::value_type slice = extensions::graphdb::schema::page_size * extensions::bitarray::cellsize;
        schema::graph_vtx_set_key_t::value_type tail = vtx / slice + 1;  // ie. the page in the list of pages, page 0 is reserved for header
        schema::graph_vtx_set_key_t::value_type map  = vtx % slice;  // ie. the index of the vertex in the page
        schema::graph_vtx_set_key_t::value_type cell = map / extensions::bitarray::cellsize;  // ie. the cell in the page

        return {tail, cell, map};
    }
}  // namespace bitarray

namespace graph  // Transactions that manage the graph.
{

    static void init(schema::TransactionNode const& parent, schema::graph_vtx_set_key_t graph)
    {
        /*
         * Initialise the graph in the DB, by creating only the vertex set. The
         * ADJ MTX is sparse. When the graph is present, do nothing.
         */

        int rc = 0;

        schema::TransactionNode session{parent, graphdb::flags::txn::NESTED_RW};

        {
            extensions::graphdb::Cursor cursor(session.txn_, session.vtx_set_.main_);

            using iter_t = extensions::graphdb::schema::graph_vtx_set_key_t;
            using hint_t = extensions::graphdb::schema::list_key_t;
            using value_t = uint8_t;

            iter_t iter = graph;
            hint_t hint = {0,0};
            torch::TensorOptions options = torch::TensorOptions().dtype(torch::kUInt8)
                                                                 .requires_grad(false);
            torch::Tensor page = torch::zeros(extensions::graphdb::schema::page_size, options);

            rc = cursor.get(iter, hint, MDB_cursor_op::MDB_SET);
            if(MDB_SUCCESS == rc) goto COMMIT;

            extensions::graphdb::list::head::find(session.vtx_set_.list_, hint);
            extensions::graphdb::refcount::increase(session.vtx_set_, iter, hint);
            extensions::graphdb::list::expand(session.vtx_set_.list_, hint, page);
        }
    COMMIT:
        return;
    }

    static schema::list_key_t::value_type is_available(schema::TransactionNode const& parent,
                                                       schema::graph_vtx_set_key_t graph,
                                                       schema::list_key_t::value_type hint)
    {
        /*
         * Query the DB for an available vertex index.
         */

        schema::TransactionNode session{parent, graphdb::flags::txn::NESTED_RW};

        int rc = 0;

        schema::list_key_t::value_type N = 0;
        schema::list_key_t::value_type max = 0;
        schema::list_key_t iter;

        torch::TensorOptions options = torch::TensorOptions().dtype(torch::kUInt8)
                                                             .requires_grad(false);
        torch::Tensor page = torch::zeros(extensions::graphdb::schema::page_size, options);

        {
            // calculate the total number of pages and vertices
            assertm(MDB_SUCCESS == (rc = session.vtx_set_.main_.get(graph, iter)), rc);

            schema::list_hdr_t hdr;
            assertm(MDB_SUCCESS == (rc = session.vtx_set_.list_.get(iter, hdr)), rc);
            N = hdr.length();
            max = hdr.bytes();
            max = max * extensions::bitarray::cellsize - 1;

            assertm(max >= hint, "Hint ", hint, " is out of bounds [0,", max, ").");

            // The graph should have been initialised.
            assertm(N > 0, N);
        }

        schema::list_key_t::value_type slice = extensions::graphdb::schema::page_size * extensions::bitarray::cellsize;

        /**
         * A discussion of the algorithm used can be found at the top of this file.
         */

        schema::list_key_t::value_type ret = hint;
        schema::list_key_t::value_type step = 0;

        std::random_device r;
        std::default_random_engine e(r());
        std::uniform_int_distribution<schema::list_key_t::value_type> d(0, max);

        iter.tail() = -1ULL;  // force initialisation of the tensor at first iteration

        while(true)
        {
            // ret is in relation to max, ie. the total number of vertices in the vertex set
            auto [tail, cell, vtx] = extensions::graphdb::bitarray::map_vtx_to_page(ret);

            if(tail != iter.tail())
            {
                iter.tail() = tail;
                rc = session.vtx_set_.list_.get(iter, page);
                assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
                if(MDB_NOTFOUND == rc)
                {
                    /**
                     * The ADJ MTX is not expanded, it is sparse and pages are
                     * created when accessed. This allows us to scale as a) large
                     * transactions cause the DB file to grow significantly and
                     * raise MDB_MAP_FULL, or b) large transactions raise
                     * MDB_TXN_FULL or c) the application blocks on IO.
                     *
                     * The vertex set is enough to guide us while the adjacency
                     * matrix is sparse.
                     */

                    page = torch::zeros(extensions::graphdb::schema::page_size, options);
                    iter = {iter.head(), 0};
                    extensions::graphdb::list::expand(session.vtx_set_.list_, iter, page);
                }
            }
            if(!extensions::bitarray::get(page, vtx)) break;
            if(step >= 0.5 * slice)  // search only one page
            {
                ret = max+1;  // should trigger expansion
            }
            else
            {
                ret = d(e);
                step += 1;
            }
        }
        return ret;
    }

    static bool vertex(schema::TransactionNode const& parent,
                       schema::graph_vtx_set_key_t graph,
                       schema::graph_vtx_set_key_t::value_type vertex)
    {
        schema::TransactionNode session{parent, graphdb::flags::txn::NESTED_RW};

        int rc = 0;
        auto [tail, cell, vtx] = extensions::graphdb::bitarray::map_vtx_to_page(vertex);

        extensions::graphdb::schema::list_key_t iter;
        assertm(MDB_SUCCESS == (rc = session.vtx_set_.main_.get(graph, iter)), rc, graph);
        iter.tail() = tail;

        torch::TensorOptions options = torch::TensorOptions().dtype(torch::kUInt8)
                                                             .requires_grad(false);
        torch::Tensor page = torch::zeros(extensions::graphdb::schema::page_size, options);

        assertm(MDB_SUCCESS == (rc = session.vtx_set_.list_.get(iter, page)), rc, iter, vertex);
        return extensions::bitarray::get(page, vtx);
    }

    static void vertex(schema::TransactionNode const& parent,
                       schema::graph_vtx_set_key_t graph,
                       schema::graph_vtx_set_key_t::value_type vertex,
                       bool truth)
    {
        schema::TransactionNode session{parent, graphdb::flags::txn::NESTED_RW};

        int rc = 0;
        auto [tail, cell, vtx] = extensions::graphdb::bitarray::map_vtx_to_page(vertex);

        extensions::graphdb::schema::list_key_t iter;
        assertm(MDB_SUCCESS == (rc = session.vtx_set_.main_.get(graph, iter)), rc, graph);
        iter.tail() = tail;

        torch::TensorOptions options = torch::TensorOptions().dtype(torch::kUInt8)
                                                             .requires_grad(false);
        torch::Tensor page = torch::zeros(extensions::graphdb::schema::page_size, options);

        assertm(MDB_SUCCESS == (rc = session.vtx_set_.list_.get(iter, page)), rc, iter, vertex);
        extensions::bitarray::set(page, vtx, truth);
        assertm(MDB_SUCCESS == (rc = session.vtx_set_.list_.put(iter, page, extensions::graphdb::flags::put::DEFAULT)), rc, iter);
    }

    static bool edge(schema::TransactionNode const& parent,
                     schema::graph_vtx_set_key_t graph,
                     schema::graph_vtx_set_key_t::value_type vertex_i,
                     schema::graph_vtx_set_key_t::value_type vertex_j)
    {
        schema::TransactionNode session{parent, graphdb::flags::txn::NESTED_RW};
        auto [tail, cell, vtx] = extensions::graphdb::bitarray::map_vtx_to_page(vertex_j);

        torch::TensorOptions options = torch::TensorOptions().dtype(torch::kUInt8)
                                                             .requires_grad(false);
        torch::Tensor page = torch::zeros(extensions::graphdb::schema::page_size, options);

        // NB. checking here for the presence of vertices is unecessary overhead

        int rc = 0;

        graphdb::schema::graph_adj_mtx_key_t key = {graph.graph(), vertex_i};
        extensions::graphdb::schema::list_key_t iter;

        rc = session.adj_mtx_.main_.get(key, iter);
        assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, key);
        if(MDB_SUCCESS == rc)
        {
            iter.tail() = tail;
            rc = session.adj_mtx_.list_.get(iter, page);
            assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
        }
        return extensions::bitarray::get(page, vtx);
    }

    static void edge(schema::TransactionNode const& parent,
                     schema::graph_vtx_set_key_t graph,
                     schema::graph_vtx_set_key_t::value_type vertex_i,
                     schema::graph_vtx_set_key_t::value_type vertex_j,
                     bool truth)
    {
        schema::TransactionNode session{parent, graphdb::flags::txn::NESTED_RW};
        auto [tail, cell, vtx] = extensions::graphdb::bitarray::map_vtx_to_page(vertex_j);

        torch::TensorOptions options = torch::TensorOptions().dtype(torch::kUInt8)
                                                             .requires_grad(false);
        torch::Tensor page = torch::zeros(extensions::graphdb::schema::page_size, options);

        assert(graph::vertex(session, graph, vertex_i));
        assert(graph::vertex(session, graph, vertex_j));

        int rc = 0;

        graphdb::schema::graph_adj_mtx_key_t key = {graph.graph(), vertex_i};
        extensions::graphdb::schema::list_key_t iter;

        rc = session.adj_mtx_.main_.get(key, iter);
        assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
        if(MDB_NOTFOUND == rc)
        {
            extensions::graphdb::list::head::find(session.adj_mtx_.list_, iter);
            extensions::graphdb::refcount::increase(session.adj_mtx_, key, iter);
        }

        iter.tail() = tail;
        rc = session.adj_mtx_.list_.get(iter, page);
        assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
        extensions::bitarray::set(page, vtx, truth);
        assertm(MDB_SUCCESS == (rc = session.adj_mtx_.list_.put(iter, page, extensions::graphdb::flags::put::DEFAULT)), rc, iter);

        {
            /**
             * The vertex set might have been expanded since the last time we
             * wrote to the adj mtx. Update the end node of this list to follow
             * the size of the vtx set.
             */

            graphdb::schema::list_key_t head;
            graphdb::schema::list_hdr_t hdr;

            // get the end node of the vtx set
            assertm(MDB_SUCCESS == (rc = session.vtx_set_.main_.get(graph, head)), rc);
            assertm(MDB_SUCCESS == (rc = session.vtx_set_.list_.get(head, hdr)), rc);

            // There should only be one reference to the vtx set, and similarly
            // to each row of the adj mtx.
            assertm(1 == hdr.refcount(), hdr);

            // copy the end node to the adj mtx list
            head = {iter.head(), 0};  // switch to the adj mtx list
            assertm(MDB_SUCCESS == (rc = session.adj_mtx_.list_.put(head, hdr, flags::put::DEFAULT)), rc);
        }
    }

    using vertices_visitor_t = std::function<void(schema::graph_vtx_set_key_t::value_type /* vertex */)>;
    using edges_visitor_t = std::function<void(schema::graph_vtx_set_key_t::value_type /* vertex */, schema::graph_vtx_set_key_t::value_type /* vertex */)>;

    static void vertices(schema::TransactionNode const& parent,
                         schema::graph_vtx_set_key_t graph,
                         vertices_visitor_t const& visitor)
    {
        // NB. does not create a child transaction as a) it does not modify the DB, 
        // b) gives an opportunity to the visitor to use the parent transaction.
        int rc = 0;

        torch::TensorOptions options = torch::TensorOptions().dtype(torch::kUInt8)
                                                             .requires_grad(false);
        torch::Tensor page = torch::zeros(extensions::graphdb::schema::page_size, options);

        schema::graph_vtx_set_key_t::value_type slice = extensions::graphdb::schema::page_size * extensions::bitarray::cellsize;

        extensions::graphdb::schema::list_key_t iter;
        assertm(MDB_SUCCESS == (rc = parent.vtx_set_.main_.get(graph, iter)), rc);
        schema::list_hdr_t hdr;

        graphdb::Cursor cursor(parent.txn_, parent.vtx_set_.list_);

        for(size_t i = 0;; i++)
        {
            iter.tail() = i;
            if(!i)
            {
                assertm(MDB_SUCCESS == (rc = cursor.get(iter, hdr, MDB_cursor_op::MDB_SET)), rc, iter);
                assertm(0 == hdr.hash(), hdr, iter);
                assertm(0 < hdr.length(), hdr, iter);
                assertm(1 == hdr.refcount(), hdr, iter);
            }
            else
            {
                rc = cursor.get(iter, page, MDB_cursor_op::MDB_NEXT);
                assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, hdr, iter);
                if(MDB_NOTFOUND == rc) break;
                for(size_t vtx = 0; vtx < slice; vtx++)
                {
                    if(extensions::bitarray::get(page, vtx))
                        visitor(slice * (i-1) + vtx);
                }
            }
        }
    }

    static void edges(schema::TransactionNode const& parent,
                      schema::graph_vtx_set_key_t graph,
                      edges_visitor_t const& visitor)
    {
        // NB. does not create a child transaction as a) it does not modify the DB, 
        // b) gives an opportunity to the visitor to use the parent transaction.
        int rc = 0;

        torch::TensorOptions options = torch::TensorOptions().dtype(torch::kUInt8)
                                                             .requires_grad(false);
        torch::Tensor page = torch::zeros(extensions::graphdb::schema::page_size, options);

        schema::graph_vtx_set_key_t::value_type slice = extensions::graphdb::schema::page_size * extensions::bitarray::cellsize;

        vertices_visitor_t vtx_set_visitor = [&](schema::graph_vtx_set_key_t::value_type vtx)
        {
            extensions::graphdb::schema::graph_adj_mtx_key_t row = {graph.graph(), vtx};
            extensions::graphdb::schema::list_key_t iter;
            rc = parent.adj_mtx_.main_.get(row, iter);
            assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, iter);
            if(MDB_NOTFOUND == rc) return;

            graphdb::schema::list_hdr_t hdr;  // NB. the adj mtx is sparse
            assertm(MDB_SUCCESS == (rc = parent.adj_mtx_.list_.get(iter, hdr)), rc);

            for(size_t i = iter.tail(); i <= hdr.length(); i++)
            {
                if(!i) continue;
                iter.tail() = i;
                rc = parent.adj_mtx_.list_.get(iter, page);
                assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, iter);
                if(MDB_NOTFOUND == rc) continue;
                for(size_t vtx_n = 0; vtx_n < slice; vtx_n++)
                {
                    if(extensions::bitarray::get(page, vtx_n))
                        visitor(vtx, slice * (i-1) + vtx_n);
                }
            }
        };
        vertices(parent, graph, vtx_set_visitor);
    }

    void read(extensions::graphdb::Environment& env,
              extensions::graph::Graph graph,
              schema::graph_vtx_set_key_t graph_i);
    void write(extensions::graphdb::Environment& env,
               extensions::graph::Graph graph,
               schema::graph_vtx_set_key_t graph_i);
}  // namespace graph

namespace stat
{

}  // namespace stat

}}  // namespace extensions::graphdb

#endif  // EXTENSIONS_GRAPHDB_GRAPHDB_H
