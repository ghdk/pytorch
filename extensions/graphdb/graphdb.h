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

    const schema::meta_page_t page_sz = extensions::graphdb::schema::page_size * CHAR_BIT;
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
 * Manages the end node of a list.
 */
template<typename K, typename = schema::is_list_key_t<K>>
void end(graphdb::Database const& parent,
         graphdb::mdb_view<K> last,     // point at the last node of a list
         graphdb::mdb_view<size_t> sz)  // size in bytes of the held buffer
{
    assertm(last.begin()->head(), last.begin()->head());
    assertm(*(sz.begin()), *(sz.begin()));

    int rc;
    typename schema::list_end_t end = {last.begin()->tail() + 1,
                                       *(sz.begin())};
    assertm(end.length() && end.bytes(), *(last.begin()), end);

    K iter = {last.begin()->head(), schema::LIST_TAIL_MAX};

    mdb_view<K> iter_v(&iter, 1);
    mdb_view<typename schema::list_end_t::value_type> end_v{end.data(), end.size()};

    assertm(MDB_SUCCESS == (rc = parent.put(iter_v, end_v, flags::put::DEFAULT)), rc, iter);
}

template<typename K>
void end(graphdb::Database const& parent, K& last, size_t sz)
{
    graphdb::mdb_view<K> last_v(&last, 1);
    graphdb::mdb_view<size_t> sz_v(&sz, 1);
    end(parent, last_v, sz_v);
}

/**
 * Through the iter returns the length of the list, through sz the total size
 * of the stored buffer.
 */
template<typename K, typename = schema::is_list_key_t<K>>
void size(graphdb::Database const& parent, graphdb::mdb_view<K> iter, mdb_view<size_t> sz)
{
    assertm(0 != iter.begin()->head(), iter.begin()->head());

    int rc = 0;

    iter.begin()->tail() = schema::LIST_TAIL_MAX;
    mdb_view<schema::list_end_t> end_v;

    rc = parent.get(iter, end_v);
    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, *(iter.begin()));

    if(MDB_SUCCESS == rc)
    {
        assertm(end_v.begin()->length() && end_v.begin()->bytes(), *(iter.begin()), *(end_v.begin()));

        iter.begin()->tail() = end_v.begin()->length();
        *(sz.begin()) = end_v.begin()->bytes();
    }
    else
    {
        iter.begin()->tail() = 0;
        *(sz.begin()) = 0;
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
 * Removes a linked list from the DB, including its end node, starting at the
 * given node.
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

    iter.tail() = schema::LIST_TAIL_MAX;
    rc = parent.remove(iter_k);
    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, iter);
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

    // update the end node
    iter.tail() -= 1;  // point to the last node
    end(parent, iter, value.size() * sizeof(V));

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
        size_t chunk = extensions::graphdb::schema::page_size / sizeof(V);

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
    assertm(value.size() * sizeof(V) <= extensions::graphdb::schema::page_size, value.size());

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
        end(parent, iter, (iter.tail() * value.size() + value.size()) * sizeof(V));
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
        end(parent, iter, iter.tail() * sz + sz);
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

/**
 * We hash the values through mdb_view, as python needs to pass py::bytes,
 * so we need to have a uniform hash value for all types.
 */
template<typename V>
size_t make_hash(V const& data)
{
    size_t ret = 0;
    if constexpr(std::is_same_v<V, torch::Tensor>)
    {
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
void write(schema::DatabaseSet const& parent, graphdb::mdb_view<K> hash, graphdb::mdb_view<V> value)
{
    assertm(0 != hash.begin()->head(), hash.begin()->head());
    assertm(0 == hash.begin()->tail(), hash.begin()->tail());

    list::expand(parent.hash_, hash, value);
}

template<typename K, typename V>
void write(schema::DatabaseSet const& parent, K& hash, V& value)
{
    graphdb::mdb_view<K> hash_v(&hash, 1);
    graphdb::mdb_view<V> value_v(&value, 1);
    write(parent, hash_v, value_v);
}

template<typename K, typename = schema::is_key_t<K>>
using visitor_t = std::function<int(K)>;

template<typename V, typename K, typename = schema::is_key_t<K>>
int visit(schema::DatabaseSet const& parent, V& value, visitor_t<K> visitor)
{
    int rc = MDB_SUCCESS;

    graphdb::schema::list_key_t hash = {0,0};
    hash.head() = make_hash(value);

    K key;

    for(size_t i = hash.tail(); i < schema::LIST_TAIL_MAX; i++)
    {
        hash.tail() = i;
        rc = parent.hash_.get(hash, key);
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
void write(schema::DatabaseSet const& parent, K& key, V& value, bool hashed)
{
    int rc = 0;

    if(hashed)
    {
        schema::list_key_t hash = {0,0};
        hash.head() = hash::make_hash(value);
        extensions::graphdb::hash::write(parent, hash, key);
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

    /**
     * Updates the iter with the head of the list. If the cached key and iter
     * point to the same page becomes a noop. The page is loaded into the passed
     * tensor.
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

        if(key == key_cached && iter == iter_cached && 0 != iter.head() && schema::LIST_TAIL_MAX != iter.head())
            return ret;  // ie. the page has been loaded, or when LIST_TAIL_MAX, an empty page has been returned

        if(key != key_cached || 0 == iter.head() || schema::LIST_TAIL_MAX == iter.head())
        {
            schema::list_key_t head = {0,0};
            rc = parent.main_.get(key, head);  // get head of list
            assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, key);
            if(MDB_SUCCESS == rc)
                iter.head() = head.head();
            else if(MDB_NOTFOUND == rc && key == key_cached && schema::LIST_TAIL_MAX == iter.head())
            {
                rc = MDB_SUCCESS;
                goto END;  // ie. noop, an empty page has already been returned
            }
            else if(MDB_NOTFOUND == rc)
            {
                std::array<uint8_t, extensions::graphdb::schema::page_size> buff = {0};
                torch::Tensor blob = torch::from_blob(buff.data(), page.sizes(), page.options());
                page.copy_(blob);
                iter.head() = schema::LIST_TAIL_MAX;
                rc = MDB_SUCCESS;
                goto END;  // ie. do not attempt to read the page from the DB
            }
        }

        /**
         * A NOTFOUND here means that the iter was not in the DB, ie. we have
         * reached the end of the linked list.
         */
        rc = parent.list_.get(iter, page);
        assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);

    END:
        if(MDB_SUCCESS == rc)
        {
            key_cached = key;
            iter_cached = iter;
        }

        ret = rc;
        return ret;
    }

    template<typename K, typename = schema::is_key_t<K>>
    static void write_page(schema::DatabaseSet const& vtx_set,
                           schema::DatabaseSet const& parent,
                           K key,
                           schema::list_key_t& iter,
                           torch::Tensor& page)
    {
        int rc = 0;

        schema::list_key_t head = {0,0};
        rc = parent.main_.get(key, head);  // get head of list
        assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc, key);
        if(MDB_NOTFOUND == rc)
        {
            extensions::graphdb::list::find_head(parent.list_, head);
            assertm(MDB_SUCCESS == (rc = parent.main_.put(key, head, extensions::graphdb::flags::put::DEFAULT)), rc);
        }

        {
            /**
             * The vertex set might have been extended since the last time we
             * wrote to the adj mtx. Update the end node of this list to follow
             * the size of the vtx set.
             */

            schema::graph_vtx_set_key_t vtx_set_key = {key.graph()};
            graphdb::schema::list_key_t list_iter;
            graphdb::schema::list_end_t list_end;

            // get the end node of the vtx set
            assertm(MDB_SUCCESS == (rc = vtx_set.main_.get(vtx_set_key, list_iter)), rc);
            list_iter.tail() = schema::LIST_TAIL_MAX;
            assertm(MDB_SUCCESS == (rc = vtx_set.list_.get(list_iter, list_end)), rc);

            // copy the end node to the adj mtx list
            list_iter.head() = head.head();  // switch to the adj mtx list
            assertm(MDB_SUCCESS == (rc = parent.list_.put(list_iter, list_end, extensions::graphdb::flags::put::DEFAULT)), rc);
        }

        iter.head() = head.head();
        assertm(MDB_SUCCESS == (rc = parent.list_.put(iter, page, extensions::graphdb::flags::put::DEFAULT)), rc, iter);
    }
}  // namespace bitarray

namespace stat
{

namespace graph
{
    inline std::string size(schema::TransactionNode const& parent, schema::graph_vtx_set_key_t key)
    {
        int rc = 0;
        std::ostringstream ret;
        size_t sz;
        extensions::graphdb::schema::list_key_t iter;

        assertm(MDB_SUCCESS == (rc = parent.vtx_set_.main_.get(key, iter)), rc, key);  // get head of list
        extensions::graphdb::list::size(parent.vtx_set_.list_, iter, sz);
        ret << "vtx set=(" << iter.tail() << "," << sz << ")";

        size_t adj_mtx_pages = 0;
        size_t adj_mtx_size = 0;
        for(size_t i = 0; i < sz; i++)
        {
            schema::graph_adj_mtx_key_t query = {key.graph(), i};
            assertm(MDB_SUCCESS == (rc = parent.adj_mtx_.main_.get(query, iter)), rc, query);  // get head of list
            extensions::graphdb::list::size(parent.adj_mtx_.list_, iter, sz);
            adj_mtx_pages += iter.tail();
            adj_mtx_size += sz;
        }
        ret << ", adj mtx=(" << adj_mtx_pages << "," << adj_mtx_size << ")";
        return ret.str();
    }
}  // namespace graph

}  // namespace stat

}}  // namespace extensions::graphdb

#endif  // EXTENSIONS_GRAPHDB_GRAPHDB_H
