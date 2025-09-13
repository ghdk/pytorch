#ifndef EXTENSIONS_GRAPH_STORAGE_H
#define EXTENSIONS_GRAPH_STORAGE_H

namespace extensions { namespace graph { namespace storage
{

using vertices_visitor_t = std::function<void(feature::index_t)>;
using edges_visitor_t = std::function<void(feature::index_t, feature::index_t)>;

class Memory
{
public:

    bool vertex(feature::index_t i)
    {
        return bitarray::get(vertices_, i);
    }
    feature::index_t vertex(feature::index_t index, bool truth)
    {
        const feature::index_t max = bitarray::size(vertices_) - 1;

        assertm(max >= index, "Index ", index, " is out of bounds [0,", max, ").");
        feature::index_t ret = index;

        if(truth)
        {

            /**
             * Finds the next available slot in the bitmap, following the open
             * addressing algorithm. If there is a collision at the given index
             * do a) select a random number r in [0, N*PAGE_SIZE*CHAR_BIT]
             * where N is the number of pages, b) if there is a collision at r
             * move forward until the end of the page (PAGE_SIZE*CHAR_BIT).
             * If a free slot cannot be found, expand the bitmap by one page
             * and place the vertex in the new page at r%(PAGE_SIZE*CHAR_BIT).
             */

            bool needs_expansion = false;
            feature::index_t step = 0;
            feature::index_t slice = extensions::graphdb::schema::page_size * extensions::bitarray::cellsize;

            std::random_device r;
            std::default_random_engine e(r());
            std::uniform_int_distribution<feature::index_t> d(0, max);

            while(true)
            {
                if(ret + step <= max && !bitarray::get(vertices_, ret + step)) break;
                if((ret || step) && !((ret + step) % slice))
                {
                    needs_expansion = true;
                    step = 0;
                    ret = max + 1 + (ret % slice);
                    break;
                }
                if(ret == index)
                {
                    ret = d(e);
                    step = 0;
                }
                else
                    step += 1;
            }
            if(needs_expansion)
            {
                torch::Tensor vertices = torch::full(vertices_.sizes()[0] + extensions::graphdb::schema::page_size,
                                                     0,
                                                     vertices_.options());
                torch::Tensor edges = torch::full({bitarray::size(vertices), vertices.sizes()[0]},
                                                  0,
                                                  edges_.options());
                vertices.slice(0 /* rows */,
                               0 /* start row */,
                               vertices_.sizes()[0] /* end row */) = vertices_.slice(0, 0, vertices_.sizes()[0]);
                edges.slice(0 /* rows */,
                            0 /* start row */,
                            edges_.sizes()[0] /* end row */)
                     .slice(1 /* columns */,
                            0 /* start column */,
                            edges_.sizes()[1] /* end column */) = edges_.slice(0, 0, edges_.sizes()[0])
                                                                        .slice(1, 0, edges_.sizes()[1]);
                vertices_ = vertices;
                edges_ = edges;
            }
            ret = ret + step;
            bitarray::set(vertices_, ret, truth);
        }
        else  // !truth
        {
            bitarray::set(vertices_, ret, truth);
            auto adjm = edges_.accessor<bitarray::cell_t, 2>();
            for(size_t i = 0; i < bitarray::size(vertices_); ++i)
            {
                if(i == ret)
                {
                    for(size_t k = 0; k < adjm[i].size(0); ++k)
                        adjm[i][k] = 0;
                }
                else
                    bitarray::set(adjm[i], ret, truth);
            }
        }

        return ret;
    }
    bool edge(feature::index_t i, feature::index_t j)
    {
        return bitarray::get(edges_[i], j);
    }

    void edge(feature::index_t i, feature::index_t j, bool truth)
    {
        assertm(bitarray::get(vertices_, i), i);
        assertm(bitarray::get(vertices_, j), j);

        auto tensor = edges_[i];
        bitarray::set(tensor, j, truth);
    }

    void vertices(vertices_visitor_t& visitor, size_t start, size_t stop, size_t step)
    {
        auto vertices = vertices_.accessor<bitarray::cell_t, 1UL>();
        size_t sz = bitarray::size(vertices);
        if(!stop) stop = sz;
        for(size_t i = start; i < stop; i += step)
        {
            if(!bitarray::get(vertices, i)) continue;
            visitor(i);
        }
    }
    void edges(edges_visitor_t& visitor, size_t start, size_t stop, size_t step)
    {
        auto vertices = vertices_.accessor<bitarray::cell_t, 1UL>();
        auto edges = edges_.accessor<bitarray::cell_t, 2UL>();
        size_t sz = bitarray::size(vertices);
        if(!stop) stop = sz*sz;

        while(start < stop)
        {
            size_t v = start / sz;  // the row of the adj mtx
            size_t e = start % sz;  // the index of the edge in a row of the adj mtx
            size_t cellV = v / bitarray::cellsize;  // the cell of the vertex in the vertex set
            size_t cellE = e / bitarray::cellsize;  // the cell of the edge in the adj mtx
            auto row = edges[v];
            if(!bitarray::get(vertices, v)) goto CONTINUE;
            if(!bitarray::get(row, e)) goto CONTINUE;
            visitor(v,e);
        CONTINUE:
        if(0 == vertices[cellV])
            start += step * (((cellV+1) * bitarray::cellsize - v) * sz / step);  // skip a row of the adj mtx for each vertex in the empty cell
        else if(0 == row[cellE])
            start += step * (((cellE+1) * bitarray::cellsize - e) / step);
        else
            start += step;
        }
    }
public:

    Memory()
    {
        auto vopt = torch::TensorOptions().dtype(torch::kUInt8)
                                          .requires_grad(false);
        vertices_ = torch::zeros(extensions::graphdb::schema::page_size, vopt);
        auto eopt = torch::TensorOptions().dtype(torch::kUInt8)
                                          .requires_grad(false);
        edges_    = torch::zeros({extensions::graphdb::schema::page_size * bitarray::cellsize, extensions::graphdb::schema::page_size}, eopt);
    }

private:

    /**
     * A tensor is a thin object holding a pointer to the underlying storage.
     * Hence passing a graph object by copy is fairly cheap, as it will only
     * bump the reference counts of the underlying storage.
     */
    static_assert(8 == sizeof(torch::Tensor));

    /**
     * The graph is data agnostic, consists of bitmaps.
     *
     * The vertices bitmap, identifies the presence of a vertex. Through reallocs
     * and vertex deletions some bits will be zero. It is a 1D tensor.
     *
     * The edges bitmap is an adjacency matrix. It is a 2D tensor.
     * 
     * This storage type keeps the full bitmaps in memory.
     */
    torch::Tensor vertices_;
    torch::Tensor edges_;
};

class Database
{

    /*
     * A thin API that forwards calls to the DB. It loads only the page
     * that the indexes map to.
     *
     * On parallelism: The graph is not thread safe. We can have a graph per
     * thread, but it will not scale nicely as each graph opens transactions
     * to the DB.
     */

private:
    using vtx_set_key_t = extensions::graphdb::schema::graph_vtx_set_key_t;
    using adj_mtx_key_t = extensions::graphdb::schema::graph_adj_mtx_key_t;
    using iter_t = extensions::graphdb::schema::list_key_t;

public:

    bool vertex_impl(extensions::graphdb::schema::DatabaseSet const& parent, feature::index_t index)
    {
        int rc = 0;
        bool ret = false;

        auto [tail, cell, vtx] = extensions::graphdb::bitarray::map_vtx_to_page(index);

        iter_t query = {vtx_set_iter_.head(), tail};
        assertm(MDB_SUCCESS == (rc = extensions::graphdb::bitarray::load_page(parent, vtx_set_key_, query, vtx_set_key_, vtx_set_iter_, vtx_set_page_)), rc, vtx_set_key_, query);
        ret = bitarray::get(vtx_set_page_, vtx);

        return ret;
    }

    bool vertex(feature::index_t index)
    {
        bool ret = false;

        ret = vertex_impl(parent_.vtx_set_, index);
        return ret;
    }
    feature::index_t vertex(feature::index_t index, bool truth)
    {
        graphdb::schema::TransactionNode session{parent_, graphdb::flags::txn::NESTED_RW};

        int rc = 0;
        feature::index_t ret = index;

        extensions::graphdb::schema::meta_page_t N = 0;
        feature::index_t max = 0;
        iter_t iter;
        {
            // calculate the total number of pages and vertices
            assertm(MDB_SUCCESS == (rc = session.vtx_set_.main_.get(vtx_set_key_, iter)), rc);

            extensions::graphdb::list::size(session.vtx_set_.list_, iter, max);
            N = iter.tail();
            max = max * extensions::bitarray::cellsize - 1;

            assertm(max >= index, "Index ", index, " is out of bounds [0,", max, ").");

            // The graph is initialised in the constructor.
            assertm(N > 0, N);
        }

        feature::index_t slice = extensions::graphdb::schema::page_size * extensions::bitarray::cellsize;

        if(truth)
        {

            /**
             * A discussion of the algorithm used can be found in Memory::vertex
             * method.
             */

            bool needs_expansion = false;
            feature::index_t step = 0;

            std::random_device r;
            std::default_random_engine e(r());
            std::uniform_int_distribution<feature::index_t> d(0, max);

            iter.tail() = -1ULL;  // force initialisation of the tensor at first iteration

            while(true)
            {
                // ret,step are in relation to max, ie. the total number of vertices in the vertex set
                auto [tail, cell, vtx] = extensions::graphdb::bitarray::map_vtx_to_page(ret+step);

                if(ret + step <= max && tail != iter.tail())
                {
                    iter.tail() = tail;
                    assertm(MDB_SUCCESS == (rc = extensions::graphdb::bitarray::load_page(session.vtx_set_, vtx_set_key_, iter, vtx_set_key_, vtx_set_iter_, vtx_set_page_)), rc, vtx_set_key_, iter);
                }
                if(ret + step <= max && !bitarray::get(vtx_set_page_, vtx)) break;
                if((ret || step) && !((ret + step) % slice))
                {
                    needs_expansion = true;
                    step = 0;
                    ret = max + 1 + (ret % slice);
                    break;
                }
                if(ret == index)
                {
                    ret = d(e);
                    step = 0;
                }
                else
                    step += 1;
            }
            ret = ret + step;  // ie. return ret in relation to max, not the slice.
            if(needs_expansion)
            {
                torch::Tensor page = torch::zeros(extensions::graphdb::schema::page_size, options_);

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

                // Expand the vertex set.
                bitarray::set(page, ret % slice, truth);

                iter = {vtx_set_iter_.head(), N-1};
                extensions::graphdb::list::expand(session.vtx_set_.list_, iter, page);

            }
            else  // doesn't need expansion
            {
                bitarray::set(vtx_set_page_, ret % slice, truth);

                assertm(MDB_SUCCESS == (rc = session.vtx_set_.list_.put(iter, vtx_set_page_, extensions::graphdb::flags::put::DEFAULT)), rc);
            }
        }
        else  // !truth
        {
            auto [tail, cell, vtx] = extensions::graphdb::bitarray::map_vtx_to_page(ret);

            iter.tail() = tail;
            assertm(MDB_SUCCESS == (rc = extensions::graphdb::bitarray::load_page(session.vtx_set_, vtx_set_key_, iter, vtx_set_key_, vtx_set_iter_, vtx_set_page_)), rc, vtx_set_key_, iter);

            bitarray::set(vtx_set_page_, vtx, truth);
            assertm(MDB_SUCCESS == (rc = session.vtx_set_.list_.put(iter, vtx_set_page_, extensions::graphdb::flags::put::DEFAULT)), rc);
        }
        return ret;
    }
    bool edge(feature::index_t i, feature::index_t j)
    {
        int rc = 0;
        bool ret = false;

        {
            auto [tail, cell, vtx] = extensions::graphdb::bitarray::map_vtx_to_page(j);

            adj_mtx_key_t key = {vtx_set_key_.graph(), i};
            iter_t iter = {adj_mtx_iter_.head(), tail};
            assertm(MDB_SUCCESS == (rc = extensions::graphdb::bitarray::load_page(parent_.adj_mtx_, key, iter, adj_mtx_key_, adj_mtx_iter_, adj_mtx_page_)), rc, key, iter);

            ret = bitarray::get(adj_mtx_page_, vtx);
        }
END:
        return ret;
    }
    void edge(feature::index_t i, feature::index_t j, bool truth)
    {
        graphdb::schema::TransactionNode session{parent_, graphdb::flags::txn::NESTED_RW};

        int rc = 0;
        auto [tail, cell, vtx] = extensions::graphdb::bitarray::map_vtx_to_page(j);

        {
            rc = vertex_impl(session.vtx_set_, i);
            assertm(rc, i);
        }

        {
            rc = vertex_impl(session.vtx_set_, j);
            assertm(rc, j);
        }

        {
            adj_mtx_key_t key = {vtx_set_key_.graph(), i};
            iter_t iter = {adj_mtx_iter_.head(), tail};
            assertm(MDB_SUCCESS == (rc = extensions::graphdb::bitarray::load_page(session.adj_mtx_, key, iter, adj_mtx_key_, adj_mtx_iter_, adj_mtx_page_)), rc, key, iter);

            bitarray::set(adj_mtx_page_, vtx, truth);
            extensions::graphdb::bitarray::write_page(session.vtx_set_, session.adj_mtx_, key, iter, adj_mtx_page_);
        }
    }

    void vertices(vertices_visitor_t& visitor, size_t start, size_t stop, size_t step)
    {
        feature::index_t slice = extensions::graphdb::schema::page_size * extensions::bitarray::cellsize;
        extensions::graphdb::schema::list_key_t iter = {0,0};

        size_t sz = 0;
        extensions::graphdb::feature::visit(parent_.vtx_set_, vtx_set_key_,
                                            [&](auto key, size_t size)
                                            {
                                                iter = key;
                                                sz = size * bitarray::cellsize;
                                                return MDB_SUCCESS;
                                            });
        if(!stop) stop = sz;

        while(start < stop)
        {
            auto [tail, cell, map] = extensions::graphdb::bitarray::map_vtx_to_page(start);
            iter.tail() = tail;
            size_t v = map;
            if(MDB_NOTFOUND == extensions::graphdb::bitarray::load_page(parent_.vtx_set_, vtx_set_key_, iter, vtx_set_key_, vtx_set_iter_, vtx_set_page_))
                break;
            if(bitarray::get(vtx_set_page_, v))
                visitor(start);
            auto vtx_set_acc_ = vtx_set_page_.accessor<bitarray::cell_t, 1UL>();
            if(0 == vtx_set_acc_[cell])
                start += step * (((cell+1) * bitarray::cellsize - map) / step);
            else
                start += step;
        }
    }
    void edges(edges_visitor_t& visitor, size_t start, size_t stop, size_t step)  // start,stop,step refer to a flattened adj mtx
    {
        extensions::graphdb::schema::list_key_t vtx_set_iter = {0,0};
        extensions::graphdb::schema::graph_adj_mtx_key_t adj_mtx_key = {vtx_set_key_.graph(),0};
        extensions::graphdb::schema::list_key_t adj_mtx_iter = {0,0};

        size_t sz = 0;
        extensions::graphdb::feature::visit(parent_.vtx_set_, vtx_set_key_,
                                            [&](auto key, size_t size)
                                            {
                                                vtx_set_iter = key;
                                                sz = size * bitarray::cellsize;
                                                return MDB_SUCCESS;
                                            });
        if(!stop) stop = sz*sz;

        while(start < stop)
        {
            size_t v = start / sz;  // the row of the adj mtx
            size_t e = start % sz;  // the index of the edge in a row of the adj mtx

            auto [tailV, cellV, mapV] = extensions::graphdb::bitarray::map_vtx_to_page(v);
            auto [tailE, cellE, mapE] = extensions::graphdb::bitarray::map_vtx_to_page(e);

            vtx_set_iter.tail() = tailV;
            if(MDB_NOTFOUND == extensions::graphdb::bitarray::load_page(parent_.vtx_set_, vtx_set_key_, vtx_set_iter, vtx_set_key_, vtx_set_iter_, vtx_set_page_))
                break;
            if(!bitarray::get(vtx_set_page_, mapV)) goto CONTINUE;

            adj_mtx_key.vertex() = v;
            adj_mtx_iter.tail() = tailE;

            if(MDB_NOTFOUND == extensions::graphdb::bitarray::load_page(parent_.adj_mtx_, adj_mtx_key, adj_mtx_iter, adj_mtx_key_, adj_mtx_iter_, adj_mtx_page_))
                break;
            if(bitarray::get(adj_mtx_page_, mapE))
                visitor(v,e);

        CONTINUE:
            auto vtx_set_acc_ = vtx_set_page_.accessor<bitarray::cell_t, 1UL>();
            auto adj_mtx_acc_ = adj_mtx_page_.accessor<bitarray::cell_t, 1UL>();
            if(0 == vtx_set_acc_[cellV])
                start += step * (((cellV+1) * bitarray::cellsize - mapV) * sz / step);  // skip a row of the adj mtx for each vertex in the empty cell
            else if(0 == adj_mtx_acc_[cellE])
                start += step * (((cellE+1) * bitarray::cellsize - mapE) / step);
            else
                start += step;
        }
    }

    Database(extensions::graphdb::schema::TransactionNode const& parent, graph::feature::index_t graph)
    : parent_{parent}
    , vtx_set_key_{graph}
    , vtx_set_iter_{}
    , adj_mtx_key_{}
    , adj_mtx_iter_{}
    {
        /*
         * Initialise the graph in the DB, by creating only the vertex set. The
         * ADJ MTX is sparse.
         */

        int rc = 0;

        graphdb::schema::TransactionNode child{parent_, graphdb::flags::txn::NESTED_RW};

        {
            extensions::graphdb::Cursor cursor(child.txn_, child.vtx_set_.main_);

            using iter_t = extensions::graphdb::schema::graph_vtx_set_key_t;
            using hint_t = extensions::graphdb::schema::list_key_t;
            using value_t = uint8_t;

            iter_t iter = {graph};
            hint_t hint = {0,0};
            std::array<value_t, extensions::graphdb::schema::page_size> page = {0};

            rc = cursor.get(iter, hint, MDB_cursor_op::MDB_SET);
            if(MDB_SUCCESS == rc) goto COMMIT;

            extensions::graphdb::list::find_head(child.vtx_set_.list_, hint);
            extensions::graphdb::list::expand(child.vtx_set_.list_, hint, page);
            assertm(MDB_SUCCESS == (rc = child.vtx_set_.main_.put(iter, hint, extensions::graphdb::flags::put::DEFAULT)), rc);
        }
    COMMIT:
        return;
    }

private:
    /**
     * The graph is data agnostic, consists of bitmaps. These bitmaps might
     * span more than a single page in the DB. However, in memory we keep only
     * one page from the vertex set and one page from the adjacency matrix.
     *
     * The vertices bitmap, identifies the presence of a vertex. Through
     * expansions of the vertex set and vertex deletions some bits will be zero.
     * It is a 1D bitmap.
     *
     * The edges bitmap is an adjacency matrix. It is a 2D bitmap.
     */
    graphdb::schema::TransactionNode const& parent_;
    torch::TensorOptions options_ = torch::TensorOptions().dtype(torch::kUInt8)
                                                          .requires_grad(false);
    torch::Tensor vtx_set_page_ = torch::zeros(extensions::graphdb::schema::page_size, options_);
    vtx_set_key_t vtx_set_key_;  // holds the graph index
    iter_t vtx_set_iter_;  // points to a page from the bitset of the vertex set
    torch::Tensor adj_mtx_page_ = torch::zeros(extensions::graphdb::schema::page_size, options_);
    adj_mtx_key_t adj_mtx_key_;  // points to the vertex whose bitset is loaded
    iter_t adj_mtx_iter_;  // points to a page from the bitset of the vertex
};

using storage_variant_t = std::variant<class Memory, class Database>;

class storage_t : private storage_variant_t
{
public:
    using parent_t = storage_variant_t;
public:

    bool vertex(feature::index_t i)
    {
        auto self = static_cast<parent_t*>(this);
        bool ret;
        if(std::holds_alternative<Memory>(*self))
            ret = std::get_if<Memory>(self)->vertex(i);
        if(std::holds_alternative<Database>(*self))
            ret = std::get_if<Database>(self)->vertex(i);
        return ret;
    }
    graph::feature::index_t vertex(feature::index_t index, bool truth)
    {
        auto self = static_cast<parent_t*>(this);
        graph::feature::index_t ret;
        if(std::holds_alternative<Memory>(*self))
            ret = std::get_if<Memory>(self)->vertex(index,truth);
        if(std::holds_alternative<Database>(*self))
            ret = std::get_if<Database>(self)->vertex(index,truth);
        return ret;
    }
    bool edge(feature::index_t i, feature::index_t j)
    {
        auto self = static_cast<parent_t*>(this);
        bool ret;
        if(std::holds_alternative<Memory>(*self))
            ret = std::get_if<Memory>(self)->edge(i,j);
        if(std::holds_alternative<Database>(*self))
            ret = std::get_if<Database>(self)->edge(i,j);
        return ret;
    }
    void edge(feature::index_t i, feature::index_t j, bool truth)
    {
        auto self = static_cast<parent_t*>(this);
        if(std::holds_alternative<Memory>(*self))
            std::get_if<Memory>(self)->edge(i,j,truth);
        if(std::holds_alternative<Database>(*self))
            std::get_if<Database>(self)->edge(i,j,truth);
    }

    void vertices(vertices_visitor_t& visitor, size_t start, size_t stop, size_t step)
    {
        auto self = static_cast<parent_t*>(this);
        if(std::holds_alternative<Memory>(*self))
            return std::get_if<Memory>(self)->vertices(visitor, start, stop, step);
        if(std::holds_alternative<Database>(*self))
            return std::get_if<Database>(self)->vertices(visitor, start, stop, step);
    }
    void edges(edges_visitor_t& visitor, size_t start, size_t stop, size_t step)
    {
        auto self = static_cast<parent_t*>(this);
        if(std::holds_alternative<Memory>(*self))
            return std::get_if<Memory>(self)->edges(visitor, start, stop, step);
        if(std::holds_alternative<Database>(*self))
            return std::get_if<Database>(self)->edges(visitor, start, stop, step);
    }

    template<typename T>
    storage_t(T value)
    : parent_t(std::move(value))
    {}
};

using storage_ptr_t = ptr_t<storage_t>;

}}}  // namespace extensions::graph::storage

#endif  // EXTENSIONS_GRAPH_STORAGE_H
