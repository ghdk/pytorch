#ifndef EXTENSIONS_GRAPHDB_GRAPHDB_H
#define EXTENSIONS_GRAPHDB_GRAPHDB_H

namespace extensions { namespace graphdb {

namespace list  // Transactions that manage the linked list DBs.
{

/*
 * Finds the next available slot in the DB, following the open addressing
 * algorithm. It receives a hint as a parameter, if the key is not present
 * in the DB it returns the hint. If there is a collision, it returns a
 * random number r in [0, N*PAGE_SIZE*CHAR_BIT). If there is a collision with r,
 * then seeks forward up to PAGE_SIZE*CHAR_BIT for an empty slot.
 * If a slot cannot be found, extends N by one, ie. extends the hypothetical
 * space by one page, and returns
 * (N-1)*PAGE_SIZE*CHAR_BIT + r%(PAGE_SIZE*CHAR_BIT).
 */
template<typename K, typename V, typename = schema::is_list_key_t<K>>
void find_slot(graphdb::Database const& parent, graphdb::mdb_view<K> hint, graphdb::mdb_view<V> value)
{
    assertm(0 == hint.begin()->tail(), hint.begin()->tail());

    int rc = 0;

    typename K::value_type ret = hint.begin()->head();
    typename K::value_type step = 0;
    schema::meta_page_t N = 1;

    Transaction txn(parent.txn_.env_, parent.txn_, flags::txn::NESTED);
    Database db(txn, parent);

    mdb_view<schema::meta_key_t> meta_k(&schema::META_KEY_PAGE, 1);
    mdb_view<schema::meta_page_t> meta_v;

    rc = db.get(meta_k, meta_v);
    if(MDB_SUCCESS == rc)
        N = *(meta_v.begin());

    const schema::meta_page_t page_sz = extensions::page_size * CHAR_BIT;
    const schema::meta_page_t max = N * page_sz - 1;
    assertm(max >= ret, "Out of bounds ", ret, " ", max);

    std::random_device r;
    std::default_random_engine e(r());
    std::uniform_int_distribution<typename K::value_type> d(0, max);

    while(true)
    {
        K query = {ret + step, 0};
        if(query.head() <= max && query.head() != schema::RESERVED)
        {
            mdb_view<K> key(&query, 1);

            db.get(key, value);
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

    meta_v = mdb_view<schema::meta_page_t>(&N, 1);
    assertm(MDB_SUCCESS == (rc = db.put(meta_k, meta_v, flags::put::DEFAULT)), rc);
    assertm(MDB_SUCCESS == (rc = txn.commit()), rc);

    *(hint.begin()) = {ret + step, 0};
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
    // If instead you want to write a single page, use directly the Database.put
    // method.
    assertm(value.size() * sizeof(V) > extensions::page_size, value.size());

    int rc = 0;

    Transaction txn(parent.txn_.env_, parent.txn_, flags::txn::NESTED);
    Database db(txn, parent);

    K iter = *(head.begin());
    mdb_view<K> iter_k(&iter, 1);
    mdb_view<V> iter_v;
    size_t chunk = extensions::page_size / sizeof(V);

    for(typename K::value_type i = 0; i < schema::LIST_TAIL_MAX; i++)
    {
        if(i*chunk >= value.size()) break;
        iter.tail() = i;
        iter_v = mdb_view<V>(value.data()          + i * chunk,
                             std::min(value.size() - i * chunk, chunk));
        assertm(MDB_SUCCESS == (rc = db.put(iter_k, iter_v, flags::put::DEFAULT)), rc);
    }

    assertm(MDB_SUCCESS == (rc = txn.commit()), rc);

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

    Transaction txn(parent.txn_.env_, parent.txn_, flags::txn::NESTED);
    Database db(txn, parent);

    K iter = *(head.begin());
    mdb_view<K> iter_k(&iter, 1);
    mdb_view<V> iter_v;
    size_t chunk = extensions::page_size / sizeof(V);

    for(size_t i = 0; i < schema::LIST_TAIL_MAX; i++)
    {
        iter.tail() = i;
        rc = db.get(iter_k, iter_v);
        if(iter.tail() == head.begin()->tail())
            assertm(MDB_SUCCESS == rc ,rc);  // node must be in the DB.
        if(MDB_NOTFOUND == rc)
            break;
        assertm(value.size() >= i * chunk + iter_v.size(), i, sizeof(V), value.size());
        memcpy((void*)(value.data() + i * chunk), (void*)(iter_v.data()), iter_v.size() * sizeof(V));
    }

    assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
}

template<typename K, typename V, typename = schema::is_list_key_t<K>>
void append(graphdb::Database const& parent, graphdb::mdb_view<K> node, graphdb::mdb_view<V> value)
{
    assertm(0 != node.begin()->head(), node.begin()->head());
    assertm(value.size() * sizeof(V) <= extensions::page_size, value.size());

    Transaction txn(parent.txn_.env_, parent.txn_, flags::txn::NESTED);
    Database db(txn, parent);

    int rc = 0;
    K iter = *(node.begin());
    mdb_view<K> iter_k(&iter, 1);
    mdb_view<V> iter_v;

    for(auto i = iter.tail(); i < schema::LIST_TAIL_MAX; ++i)
    {
        iter.tail() = i;
        rc = db.get(iter_k, iter_v);
        if(iter.tail() == node.begin()->tail())
            assertm(MDB_SUCCESS == rc ,rc);  // node must be in the DB.
        if(MDB_NOTFOUND == rc)
        {
            assertm(MDB_SUCCESS == (rc = db.put(iter_k, value, flags::put::DEFAULT)), rc);
            break;
        }
    }
    assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
}

/**
 * Removes a linked list from the DB.
 */
template<typename K, typename = schema::is_list_key_t<K>>
void clear(graphdb::Database const& parent, graphdb::mdb_view<K> head)
{
    assertm(0 != head.begin()->head(), head.begin()->head());
    assertm(0 == head.begin()->tail(), head.begin()->tail());

    int rc = 0;

    Transaction txn(parent.txn_.env_, parent.txn_, flags::txn::NESTED);
    Database db(txn, parent);

    K iter = *(head.begin());
    mdb_view<K> iter_k(&iter, 1);

    for(size_t i = iter.tail(); i < schema::LIST_TAIL_MAX; i++)
    {
        iter.tail() = i;
        rc = db.remove(iter_k);
        if(iter.tail() == head.begin()->tail())
            assertm(MDB_SUCCESS == rc ,rc);  // head must be in the DB.
        if(MDB_NOTFOUND == rc)
            break;
    }

    assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
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

    Transaction txn(parent.txn_.env_, parent.txn_, flags::txn::NESTED);
    Database db(txn, parent);

    K iter = *(node.begin());
    mdb_view<K> iter_k(&iter, 1);
    mdb_view<uint8_t> iter_v;

    for(size_t i = iter.tail(); i < schema::LIST_TAIL_MAX; i++)
    {
        iter.tail() = i;
        rc = db.get(iter_k, iter_v);
        if(iter.tail() == node.begin()->tail())
        {
            assertm(MDB_SUCCESS == rc ,rc);  // key must be in the DB.
            goto DELETE;
        }
        else if(MDB_NOTFOUND == rc)
            break;
        iter.tail() = i-1;  // replace the previous node
        assertm(MDB_SUCCESS == (rc = db.put(iter_k, iter_v, flags::put::DEFAULT)), rc);
DELETE:
        iter.tail() = i;  // delete the current node
        assertm(MDB_SUCCESS == (rc = db.remove(iter_k)), rc);
    }

    assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
}

}  // namespace list

namespace graph  // Transactions that manage the graph DBs.
{
    //  FIXME: graph algorithms for adding/removing vertices and edges.
}  // namespace graph

namespace hash  // Transactions that manage the hash DBs.
{

template<typename K, typename V, typename = schema::is_list_key_t<K>, typename = schema::is_key_t<V>>
void write(graphdb::Database const& parent, graphdb::mdb_view<K> hash, graphdb::mdb_view<V> value)
{
    assertm(0 != hash.begin()->head(), hash.begin()->head());
    assertm(0 == hash.begin()->tail(), hash.begin()->tail());

    int rc = 0;

    Transaction txn(parent.txn_.env_, parent.txn_, flags::txn::NESTED);
    Database db(txn, parent);

    mdb_view<V> existing;

    rc = db.get(hash, existing);
    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);

    if(MDB_SUCCESS == rc)
        list::append(db, hash, value);
    if(MDB_NOTFOUND == rc)
        assertm(MDB_SUCCESS == (rc = db.put(hash, value, flags::put::DEFAULT)), rc);

    assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
}

}  // namespace hash

}}  // namespace extensions::graphdb

#endif  // EXTENSIONS_GRAPHDB_GRAPHDB_H
