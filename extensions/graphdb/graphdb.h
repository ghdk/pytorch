#ifndef EXTENSIONS_GRAPHDB_GRAPHDB_H
#define EXTENSIONS_GRAPHDB_GRAPHDB_H

namespace extensions { namespace graphdb {

namespace list  // Transactions that manage the linked list DBs.
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
void find_head(graphdb::Database const& parent, graphdb::mdb_view<K> hint)
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

    const schema::meta_page_t page_sz = extensions::page_size * CHAR_BIT;
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
            if((ret || step) && !(query.head() % page_sz))
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
void find_head(graphdb::Database const& parent, K& hint)
{
    mdb_view<K> hint_v(&hint, 1);
    find_head(parent, hint_v);
}

/**
 * Through the iter returns the length of the list, through sz the total size
 * of the stored buffer.
 */
template<typename K, typename = schema::is_list_key_t<K>>
void size(graphdb::Database const& parent, graphdb::mdb_view<K> iter, mdb_view<size_t> sz)
{
    int rc = 0;
    *(sz.begin()) = 0;

    assertm(0 != iter.begin()->head(), iter.begin()->head());

    {
        Cursor cursor(parent.txn_, parent);
        mdb_view<K> iter_c;
        mdb_view<uint8_t> iter_v;

        for(typename K::value_type i = 0; i < schema::LIST_TAIL_MAX; i++)
        {
            iter.begin()->tail() = i;
            iter_c = iter;
            rc = cursor.get(iter_c, iter_v, MDB_cursor_op::MDB_SET);
            if(MDB_NOTFOUND == rc) break;
            if(iter.begin()->head() != iter_c.begin()->head()) break;
            *(sz.begin()) += iter_v.size();
        }
    }

}

template<typename K>
void size(graphdb::Database const& parent, K& iter, size_t& sz)
{
    graphdb::mdb_view<K> iter_v(&iter, 1);
    graphdb::mdb_view<size_t> sz_v(&sz, 1);
    size(parent, iter_v, sz_v);
}

/**
 * Removes a linked list from the DB, starting at the given node.
 */
template<typename K, typename = schema::is_list_key_t<K>>
void clear(graphdb::Database const& parent, graphdb::mdb_view<K> head)
{
    assertm(0 != head.begin()->head(), head.begin()->head());

    int rc = 0;

    K iter = *(head.begin());
    mdb_view<K> iter_k(&iter, 1);

    for(size_t i = iter.tail(); i < schema::LIST_TAIL_MAX; i++)
    {
        iter.tail() = i;
        rc = parent.remove(iter_k);
        if(MDB_NOTFOUND == rc)
            break;
    }

}

template<typename K>
void clear(graphdb::Database const& parent, K& head)
{
    mdb_view<K> head_v(&head, 1);
    clear(parent, head_v);
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
    size_t chunk = extensions::page_size / sizeof(V);  // number of items in a page

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

}

template<typename K, typename V>
void write(graphdb::Database const& parent, K& head, V& value)
{
    const graphdb::mdb_view<K> head_v(&head, 1);

    if constexpr(std::is_same_v<V, torch::Tensor>)
    {
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
        size_t chunk = extensions::page_size / sizeof(V);

        for(size_t i = 0; i < schema::LIST_TAIL_MAX; i++)
        {
            iter.tail() = i;
            rc = cursor.get(iter_k, iter_v, MDB_cursor_op::MDB_SET);
            if(i == head.begin()->tail())
                assertm(MDB_SUCCESS == rc ,rc);  // node must be in the DB.
            if(MDB_NOTFOUND == rc) break;
            if(iter.head() != iter_k.begin()->head()) break;
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

/**
 * Expands, a potentially empty, linked list by a single node.
 */
template<typename K, typename V, typename = schema::is_list_key_t<K>>
void expand(graphdb::Database const& parent, graphdb::mdb_view<K> node, graphdb::mdb_view<V> value)
{
    assertm(0 != node.begin()->head(), node.begin()->head());
    assertm(value.size() * sizeof(V) <= extensions::page_size, value.size());

    int rc = 0;

    {
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

    for(size_t i = iter.tail(); i < schema::LIST_TAIL_MAX; i++)
    {
        iter.tail() = i;
        rc = parent.get(iter_k, iter_v);
        if(iter.tail() == node.begin()->tail())
        {
            assertm(MDB_SUCCESS == rc ,rc);  // key must be in the DB.
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
}

template<typename K>
void remove(graphdb::Database const& parent, K& node)
{
    graphdb::mdb_view<K> node_v(&node, 1);
    remove(parent, node_v);
}

}  // namespace list

namespace hash  // Transactions that manage the hash DBs.
{

template<typename K, typename V, typename = schema::is_list_key_t<K>, typename = schema::is_key_t<V>>
void write(graphdb::Database const& parent, graphdb::mdb_view<K> hash, graphdb::mdb_view<V> value)
{
    assertm(0 != hash.begin()->head(), hash.begin()->head());
    assertm(0 == hash.begin()->tail(), hash.begin()->tail());

    list::expand(parent, hash, value);
}

template<typename K, typename V>
void write(graphdb::Database const& parent, K& hash, V& value)
{
    graphdb::mdb_view<K> hash_v(&hash, 1);
    graphdb::mdb_view<V> value_v(&value, 1);
    write(parent, hash_v, value_v);
}

template<typename K, typename = schema::is_key_t<K>>
using visitor_t = std::function<int(K)>;

template<typename V, typename K, typename = schema::is_key_t<K>>
int visit(graphdb::Database const& parent, V& value, visitor_t<K> visitor)
{
    int rc = MDB_SUCCESS;

    graphdb::schema::list_key_t hash = {0,0};
    {
        if constexpr(extensions::is_contiguous<V>::value)
        {
            std::string_view v{reinterpret_cast<const char*>(value.data()), value.size()};
            hash = {std::hash<std::string_view>{}(v), 0};
        }
        else if constexpr(std::is_fundamental_v<V>)
        {
            hash = {std::hash<V>{}(value), 0};
        }
        else
            static_assert(extensions::false_always<K,V>::value);
    }

    K key;

    for(size_t i = hash.tail(); i < schema::LIST_TAIL_MAX; i++)
    {
        hash.tail() = i;
        rc = parent.get(hash, key);
        if(MDB_SUCCESS != rc) break;
        rc = visitor(key);
        if(MDB_SUCCESS == rc) break;
    }

    return rc;
}

}  // namespace hash

namespace feature
{

/**
 * A feature is read from the DB in two steps A) lookup, which gives us the
 * head of the list and the size of the buffer, B) using the head of the list
 * and the size of the buffer, we read the data from the DB.
 */

using visitor_t = std::function<int(schema::list_key_t, size_t)>;

template <typename K>
int visit(schema::DatabaseSet const& parent, K& key, visitor_t visitor)
{
    int rc = 0;
    schema::list_key_t iter;
    size_t size = 0;
    rc = parent.main_.get(key, iter);
    if(MDB_SUCCESS == rc)
    {
        extensions::graphdb::list::size(parent.list_, iter, size);
        iter.tail() = 0;
        rc = visitor(iter, size);
    }
    return rc;
}

template <typename K, typename V>
void write(schema::DatabaseSet const& parent, K& key, V& value)
{
    int rc = 0;

    schema::list_key_t hash = {0,0};
    {
        if constexpr(extensions::is_contiguous<V>::value)
        {
            std::string_view v{reinterpret_cast<const char*>(value.data()), value.size()};
            hash = {std::hash<std::string_view>{}(v), 0};
        }
        else if constexpr(std::is_fundamental_v<V>)
        {
            hash = {std::hash<std::remove_cv_t<V>>{}(value), 0};
        }
        else
            static_assert(extensions::false_always<K,V>::value);
    }

    schema::list_key_t head;
    rc = parent.main_.get(key, head);  // get head of list
    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
    if(MDB_NOTFOUND == rc)
    {
        list::find_head(parent.list_, head);
        assertm(MDB_SUCCESS == (rc = parent.main_.put(key, head, flags::put::DEFAULT)), rc);
    }
    extensions::graphdb::list::write(parent.list_, head, value);
    extensions::graphdb::hash::write(parent.hash_, hash, key);
}

}  // namespace feature

namespace bitarray  // Transactions that manage the bitmaps.
{
    static std::tuple<schema::list_key_t::value_type, schema::list_key_t::value_type, schema::list_key_t::value_type>
    map_vtx_to_page(schema::graph_vtx_set_key_t::value_type vtx)
    {
        schema::graph_vtx_set_key_t::value_type slice = extensions::page_size * extensions::bitarray::cellsize;
        schema::graph_vtx_set_key_t::value_type tail = vtx / slice;  // ie. the page in the list of pages
        schema::graph_vtx_set_key_t::value_type map  = vtx % slice;  // ie. the index of the vertex in the page
        schema::graph_vtx_set_key_t::value_type cell = map / extensions::bitarray::cellsize;  // ie. the cell in the page

        return {tail, cell, map};
    }

    /**
     * Updates the iter with the head of the list. If the cached key and iter
     * point to the same page becomes a noop. The page is loaded into the passed
     * tensor and accessor.
     */
    template<typename K, typename = schema::is_key_t<K>>
    static int load_page(schema::DatabaseSet const& parent,
                         K key,
                         schema::list_key_t& iter,
                         K& key_cached,
                         schema::list_key_t& iter_cached,
                         torch::Tensor& page)
    {
        int ret = MDB_SUCCESS, rc = 0;

        if(key == key_cached && iter == iter_cached && iter.head())
            return ret;  // ie. the page is already loaded.

        if(key != key_cached || 0 == iter.head())
        {
            schema::list_key_t head;
            assertm(MDB_SUCCESS == (rc = parent.main_.get(key, head)), rc, key);  // get head of list
            iter.head() = head.head();
        }

        rc = parent.list_.get(iter, page);
        assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);

        if(MDB_SUCCESS == rc)
        {
            key_cached = key;
            iter_cached = iter;
        }

        ret = rc;
        return ret;
    }
}  // namespace bitarray

}}  // namespace extensions::graphdb

#endif  // EXTENSIONS_GRAPHDB_GRAPHDB_H
