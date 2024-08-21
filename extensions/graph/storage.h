#ifndef EXTENSIONS_GRAPH_STORAGE_H
#define EXTENSIONS_GRAPH_STORAGE_H

namespace extensions { namespace graph { namespace storage
{

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
            feature::index_t slice = extensions::page_size * extensions::bitarray::cellsize;

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
                torch::Tensor vertices = torch::full(vertices_.sizes()[0] + extensions::page_size,
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

    extensions::iter::GeneratedEnumerable<feature::index_t> vertices()
    {
        /**
         * A generator that returns the index of each vertex.
         */
        struct generator
        {
        public:
            using gen_t = feature::index_t;
        public:
            std::optional<gen_t> operator()()
            {
                auto vertices = ref_.vertices_.accessor<bitarray::cell_t, 1UL>();

                size_t sz = bitarray::size(vertices);
                std::optional<gen_t> ret;

                for(; index_ < sz; index_++)
                {
                    if(ret) break;  // index needs to advance before we break
                    if(!bitarray::get(ref_.vertices_, index_)) continue;
                    ret = index_;
                }
                return ret;
            }

            std::optional<gen_t> operator()() const
            {
                return const_cast<generator*>(this)->operator ()();
            }
        public:
            generator(Memory const& ref)
            : ref_{ref}
            {}
        private:
            gen_t index_ = 0;
            Memory const& ref_;
        };

        return extensions::iter::generate(generator{*this});
    }
    extensions::iter::GeneratedEnumerable<std::pair<feature::index_t, feature::index_t>> edges()
    {
        /**
         * A generator that returns the indexes of the two vertices of each
         * edge.
         */
        struct generator
        {
        public:
            using gen_t = std::pair<feature::index_t, feature::index_t>;
        public:

            std::optional<gen_t> operator()()
            {
                auto vertices = ref_.vertices_.accessor<bitarray::cell_t, 1UL>();
                auto edges = ref_.edges_.accessor<bitarray::cell_t, 2UL>();

                size_t sz = bitarray::size(vertices);
                size_t index_v1 = std::get<0>(index_);
                size_t index_v2 = std::get<1>(index_);
                size_t index_c  = index_v2 / bitarray::cellsize;
                       index_v2 = index_v2 % bitarray::cellsize;

                std::optional<gen_t> ret;

                for(; index_v1 < sz; index_v1++)
                {
                    if(!bitarray::get(vertices, index_v1)) continue;
                    auto row = edges[index_v1];
                    for(index_c = index_c < row.size(0) ? index_c : 0;
                        index_c < row.size(0);
                        index_c++)
                    {
                        if(!row[index_c]) continue;
                        for(index_v2 = index_v2 < bitarray::cellsize ? index_v2 : 0;
                            index_v2 < bitarray::cellsize;
                            index_v2++)
                        {
                            if(ret) goto END;  // only index_v2 needs to advance before we break
                            if(!bitarray::get(row, index_c * bitarray::cellsize + index_v2)) continue;
                            ret = {index_v1,       index_c * bitarray::cellsize + index_v2};
                        }
                    }
                }
            END:
                std::get<0>(index_) = index_v1;
                std::get<1>(index_) = index_c * bitarray::cellsize + index_v2;
                return ret;
            }

            std::optional<gen_t> operator()() const
            {
                return const_cast<generator*>(this)->operator ()();
            }
        public:
            generator(Memory const& ref)
            : ref_{ref}
            {}
        private:
            gen_t index_ = {0,0};
            Memory const& ref_;
        };

        return extensions::iter::generate(generator{*this});
    }

public:

    Memory()
    {
        auto vopt = torch::TensorOptions().dtype(torch::kUInt8)
                                          .requires_grad(false);
        vertices_ = torch::zeros(extensions::page_size, vopt);
        auto eopt = torch::TensorOptions().dtype(torch::kUInt8)
                                          .requires_grad(false);
        edges_    = torch::zeros({extensions::page_size * bitarray::cellsize, extensions::page_size}, eopt);
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

        extensions::graphdb::Transaction txn(env_, parent.main_.txn_, extensions::graphdb::flags::txn::WRITE);
        extensions::graphdb::schema::DatabaseSet vtx_set(txn, parent);

        feature::index_t slice = extensions::page_size * extensions::bitarray::cellsize;
        iter_t::value_type tail = index / slice;  // ie. the page in the list of pages
        iter_t::value_type vtx  = index % slice;  // ie. the index of the vertex in the page

        iter_t query = {vtx_set_iter_.head(), tail};
        assertm(MDB_SUCCESS == (rc = load_vtx_set_page(vtx_set, vtx_set_key_, query)), rc, vtx_set_key_.graph(), query.head(), query.tail());
        ret = bitarray::get(vtx_set_acc_, vtx);

        assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
        return ret;
    }

    bool vertex(feature::index_t index)
    {
        int rc = 0;
        bool ret = false;

        extensions::graphdb::Transaction txn(env_, extensions::graphdb::flags::txn::WRITE);
        extensions::graphdb::schema::DatabaseSet vtx_set(txn, extensions::graphdb::schema::VERTEX_SET, extensions::graphdb::flags::db::WRITE);

        ret = vertex_impl(vtx_set, index);

        assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
        return ret;
    }
    feature::index_t vertex(feature::index_t index, bool truth)
    {

        int rc = 0;
        feature::index_t ret = index;

        extensions::graphdb::Transaction txn(env_, extensions::graphdb::flags::txn::WRITE);
        extensions::graphdb::schema::DatabaseSet vtx_set(txn, extensions::graphdb::schema::VERTEX_SET, extensions::graphdb::flags::db::WRITE);
        extensions::graphdb::schema::DatabaseSet adj_mtx(txn, extensions::graphdb::schema::ADJACENCY_MATRIX, extensions::graphdb::flags::db::WRITE);

        extensions::graphdb::schema::meta_page_t N = 0;
        feature::index_t max = 0;
        iter_t iter;
        {
            assertm(MDB_SUCCESS == (rc = vtx_set.main_.get(vtx_set_key_, iter)), rc);

            extensions::graphdb::list::size(vtx_set.list_, iter, max);
            N = iter.tail();
            max = max * extensions::bitarray::cellsize - 1;

            assertm(max >= index, "Index ", index, " is out of bounds [0,", max, ").");

            // The graph is initialised in the constructor.
            assertm(N > 0, N);
        }

        feature::index_t slice = extensions::page_size * extensions::bitarray::cellsize;

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
                iter_t::value_type tail = (ret + step) / slice;  // ie. the page in the list of pages
                iter_t::value_type vtx  = (ret + step) % slice;  // ie. the index of the vertex in the page
                if(ret + step <= max && tail != iter.tail())
                {
                    iter.tail() = tail;
                    assertm(MDB_SUCCESS == (rc = load_vtx_set_page(vtx_set, vtx_set_key_, iter)), rc, iter.head(), iter.tail());
                }
                if(ret + step <= max && !bitarray::get(vtx_set_acc_, vtx)) break;
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
                // Expand the vertex set.
                torch::Tensor page = torch::zeros(extensions::page_size, options_);
                bitarray::set(page, ret % slice, truth);

                iter.tail() = N-1;
                extensions::graphdb::list::expand(vtx_set.list_, iter, page);

                // Expand the adjacency matrix.
                using key_t = extensions::graphdb::schema::graph_adj_mtx_key_t;
                using iter_t = extensions::graphdb::schema::list_key_t;
                using value_t = extensions::bitarray::cell_t;

                for(size_t vtx = 0; vtx < (N+1) * slice; vtx++)
                {
                    key_t key = {vtx_set_key_.graph(), vtx};
                    iter_t iter = {vtx,0};
                    rc = adj_mtx.main_.get(key, iter);
                    assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);
                    if(MDB_SUCCESS == rc)
                    {
                        extensions::graphdb::list::expand(adj_mtx.list_, iter, page);
                    }
                    else if(MDB_NOTFOUND == rc)
                    {
                        extensions::graphdb::list::find_head(adj_mtx.list_, iter);
                        assertm(MDB_SUCCESS == (rc = adj_mtx.main_.put(key, iter, extensions::graphdb::flags::put::DEFAULT)), rc);
                        for(size_t p = 0; p < N+1; p++)
                        {
                            iter.tail() = p;
                            extensions::graphdb::list::expand(adj_mtx.list_, iter, page);
                        }
                    }
                }
            }
            else  // doesn't need expansion
            {
                bitarray::set(vtx_set_acc_, ret % slice, truth);

                assertm(MDB_SUCCESS == (rc = vtx_set.list_.put(iter, vtx_set_page_, extensions::graphdb::flags::put::DEFAULT)), rc);
            }
        }
        else  // !truth
        {
            iter_t::value_type tail = ret / slice;  // ie. the page in the list of pages
            iter_t::value_type vtx  = ret % slice;  // ie. the index of the vertex in the page

            iter.tail() = tail;
            assertm(MDB_SUCCESS == (rc = load_vtx_set_page(vtx_set, vtx_set_key_, iter)), rc, iter.head(), iter.tail());

            bitarray::set(vtx_set_acc_, vtx, truth);
            assertm(MDB_SUCCESS == (rc = vtx_set.list_.put(iter, vtx_set_page_, extensions::graphdb::flags::put::DEFAULT)), rc);
        }

        assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
        return ret;
    }
    bool edge(feature::index_t i, feature::index_t j)
    {
        int rc = 0;
        bool ret = false;

        extensions::graphdb::Transaction txn(env_, extensions::graphdb::flags::txn::WRITE);
        extensions::graphdb::schema::DatabaseSet vtx_set(txn, extensions::graphdb::schema::VERTEX_SET,       extensions::graphdb::flags::db::WRITE);
        extensions::graphdb::schema::DatabaseSet adj_mtx(txn, extensions::graphdb::schema::ADJACENCY_MATRIX, extensions::graphdb::flags::db::WRITE);

        feature::index_t slice = extensions::page_size * extensions::bitarray::cellsize;

        {
            iter_t::value_type tail = j / slice;  // ie. the page in the list of pages
            iter_t::value_type vtx  = j % slice;  // ie. the index of the vertex in the page

            adj_mtx_key_t key = {vtx_set_key_.graph(), i};
            iter_t iter = {adj_mtx_iter_.head(), tail};
            assertm(MDB_SUCCESS == (rc = load_adj_mtx_page(adj_mtx, key, iter)), rc, key.graph(), key.vertex(), iter.head(), iter.tail());

            ret = bitarray::get(adj_mtx_acc_, vtx);
        }
END:
        assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
        return ret;
    }
    void edge(feature::index_t i, feature::index_t j, bool truth)
    {
        int rc = 0;

        extensions::graphdb::Transaction txn(env_, extensions::graphdb::flags::txn::WRITE);
        extensions::graphdb::schema::DatabaseSet vtx_set(txn, extensions::graphdb::schema::VERTEX_SET,       extensions::graphdb::flags::db::WRITE);
        extensions::graphdb::schema::DatabaseSet adj_mtx(txn, extensions::graphdb::schema::ADJACENCY_MATRIX, extensions::graphdb::flags::db::WRITE);

        feature::index_t slice = extensions::page_size * extensions::bitarray::cellsize;
        iter_t::value_type tail = j / slice;  // ie. the page in the list of pages
        iter_t::value_type vtx  = j % slice;  // ie. the index of the vertex in the page

        {
            rc = vertex_impl(vtx_set, i);
            assertm(rc, i);
        }

        {
            rc = vertex_impl(vtx_set, j);
            assertm(rc, j);
        }

        {
            adj_mtx_key_t key = {vtx_set_key_.graph(), i};
            iter_t iter = {adj_mtx_iter_.head(), tail};
            assertm(MDB_SUCCESS == (rc = load_adj_mtx_page(adj_mtx, key, iter)), rc, key.graph(), key.vertex(), iter.head(), iter.tail());

            bitarray::set(adj_mtx_acc_, vtx, truth);
            assertm(MDB_SUCCESS == (rc = adj_mtx.list_.put(iter, adj_mtx_page_, extensions::graphdb::flags::put::DEFAULT)), rc, iter.head(), iter.tail());
        }

        assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
    }

    extensions::iter::GeneratedEnumerable<feature::index_t> vertices()
    {
        /**
         * A generator that returns the index of each vertex.
         */
        struct generator
        {
        public:
            using gen_t = feature::index_t;
        private:
            using iter_t = extensions::graphdb::schema::list_key_t;
        public:
            std::optional<gen_t> operator()()
            {
                int rc = 0;
                extensions::graphdb::Transaction txn(ref_.env_, extensions::graphdb::flags::txn::WRITE);
                extensions::graphdb::schema::DatabaseSet vtx_set(txn, extensions::graphdb::schema::VERTEX_SET, extensions::graphdb::flags::db::WRITE);

                feature::index_t slice = extensions::page_size * extensions::bitarray::cellsize;
                std::optional<gen_t> ret;

                for(; iter_.tail() < graphdb::schema::LIST_TAIL_MAX; iter_.tail()++)
                {
                    if(MDB_NOTFOUND == ref_.load_vtx_set_page(vtx_set, ref_.vtx_set_key_, iter_))
                        goto END;
                    for(index_ = index_ < slice ? index_ : 0;
                        index_ < slice;
                        index_++)
                    {
                        if(ret) goto END;
                        if(!bitarray::get(ref_.vtx_set_acc_, index_)) continue;
                        ret = iter_.tail() * slice + index_;
                    }
                }
            END:
                assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                return ret;
            }

            std::optional<gen_t> operator()() const
            {
                return const_cast<generator*>(this)->operator ()();
            }
        public:
            generator(Database& ref)
            : ref_{ref}
            {
                int rc = 0;
                extensions::graphdb::Transaction txn(ref_.env_, extensions::graphdb::flags::txn::WRITE);
                extensions::graphdb::schema::DatabaseSet vtx_set(txn, extensions::graphdb::schema::VERTEX_SET, extensions::graphdb::flags::db::WRITE);

                rc = ref_.load_vtx_set_page(vtx_set, ref_.vtx_set_key_, iter_);
                assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
            }
        private:
            iter_t iter_ = {0,0};
            gen_t index_ = 0;
            Database& ref_;
        };

        return extensions::iter::generate(generator{*this});
    }
    extensions::iter::GeneratedEnumerable<std::pair<feature::index_t, feature::index_t>> edges()
    {
        /**
         * A generator that returns the indexes of the two vertices of each
         * edge.
         */
        struct generator
        {
        public:
            using gen_t = std::pair<feature::index_t, feature::index_t>;
        private:
            using adj_mtx_key_t = extensions::graphdb::schema::graph_adj_mtx_key_t;
            using iter_t = extensions::graphdb::schema::list_key_t;

        public:

            std::optional<gen_t> operator()()
            {
                int rc = 0;
                extensions::graphdb::Transaction txn(ref_.env_, extensions::graphdb::flags::txn::WRITE);
                extensions::graphdb::schema::DatabaseSet vtx_set(txn, extensions::graphdb::schema::VERTEX_SET,       extensions::graphdb::flags::db::WRITE);
                extensions::graphdb::schema::DatabaseSet adj_mtx(txn, extensions::graphdb::schema::ADJACENCY_MATRIX, extensions::graphdb::flags::db::WRITE);

                feature::index_t slice = extensions::page_size * extensions::bitarray::cellsize;

                size_t index_v1 = std::get<0>(index_);
                size_t index_v2 = std::get<1>(index_);
                size_t index_c  = index_v2 / bitarray::cellsize;
                       index_v2 = index_v2 % bitarray::cellsize;

                std::optional<gen_t> ret;

                for(;; index_v1++)
                {
                    vtx_set_iter_.tail() = index_v1 / slice;
                    size_t index_v1_i = index_v1 % slice;
                    if(MDB_NOTFOUND == ref_.load_vtx_set_page(vtx_set, ref_.vtx_set_key_, vtx_set_iter_))
                        goto END;
                    if(!bitarray::get(ref_.vtx_set_acc_, index_v1_i)) continue;
                    adj_mtx_key_.vertex() = index_v1;
                    for(;; index_c++)
                    {
                        adj_mtx_iter_.tail() = index_c / extensions::page_size;
                        size_t index_c_i = index_c % extensions::page_size;
                        if(MDB_NOTFOUND == ref_.load_adj_mtx_page(adj_mtx, adj_mtx_key_, adj_mtx_iter_))
                        {
                            adj_mtx_iter_ = {0,0};
                            index_c = 0;
                            index_v2 = 0;
                            break;
                        }
                        if(!ref_.adj_mtx_acc_[index_c_i]) continue;
                        for(index_v2 = index_v2 < bitarray::cellsize ? index_v2 : 0;
                            index_v2 < bitarray::cellsize;
                            index_v2++)
                        {
                            if(ret) goto END;  // only index_v2 needs to advance before we break
                            if(!bitarray::get(ref_.adj_mtx_acc_, index_c_i * bitarray::cellsize + index_v2)) continue;
                            ret = {index_v1,                     index_c_i * bitarray::cellsize + index_v2};
                        }
                    }
                }

            END:
                std::get<0>(index_) = index_v1;
                std::get<1>(index_) = index_c * bitarray::cellsize + index_v2;
                assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                return ret;
            }

            std::optional<gen_t> operator()() const
            {
                return const_cast<generator*>(this)->operator ()();
            }
        public:
            generator(Database& ref)
            : ref_{ref}
            , adj_mtx_key_{ref_.vtx_set_key_.graph(), 0}
            {
                int rc = 0;
                extensions::graphdb::Transaction txn(ref_.env_, extensions::graphdb::flags::txn::WRITE);
                extensions::graphdb::schema::DatabaseSet vtx_set(txn, extensions::graphdb::schema::VERTEX_SET,       extensions::graphdb::flags::db::WRITE);
                extensions::graphdb::schema::DatabaseSet adj_mtx(txn, extensions::graphdb::schema::ADJACENCY_MATRIX, extensions::graphdb::flags::db::WRITE);

                assertm(MDB_SUCCESS == (rc = ref_.load_vtx_set_page(vtx_set, ref_.vtx_set_key_, vtx_set_iter_)), rc);

                assertm(MDB_SUCCESS == (rc = ref_.load_adj_mtx_page(adj_mtx, adj_mtx_key_, adj_mtx_iter_)), rc);
                assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
            }
        private:
            Database& ref_;
            iter_t vtx_set_iter_ = {0,0};
            adj_mtx_key_t adj_mtx_key_;
            iter_t adj_mtx_iter_ = {0,0};
            gen_t index_ = {0,0};
        };

        return extensions::iter::generate(generator{*this});
    }

    Database(extensions::graphdb::Environment& env, graph::feature::index_t graph)
    : env_{env}
    , vtx_set_key_{graph}
    , vtx_set_iter_{}
    , adj_mtx_key_{}
    , adj_mtx_iter_{}
    {
        /*
         * Initialise the graph in the DB,
         * - Create a single page for the vertex set in VS
         * - Create the corresponding rows of the ADJ MTX in AM
         */

        int rc = 0;

        extensions::graphdb::Transaction txn(env_, extensions::graphdb::flags::txn::WRITE);
        extensions::graphdb::schema::DatabaseSet vtx_set(txn, extensions::graphdb::schema::VERTEX_SET, extensions::graphdb::flags::db::WRITE);
        extensions::graphdb::schema::DatabaseSet adj_mtx(txn, extensions::graphdb::schema::ADJACENCY_MATRIX, extensions::graphdb::flags::db::WRITE);

        {
            extensions::graphdb::Cursor cursor(txn, vtx_set.main_);

            using iter_t = extensions::graphdb::schema::graph_vtx_set_key_t;
            using hint_t = extensions::graphdb::schema::list_key_t;
            using value_t = uint8_t;

            iter_t iter = {graph};
            hint_t hint = {0,0};
            std::array<uint8_t, extensions::page_size> page = {0};

            rc = cursor.get(iter, hint, MDB_cursor_op::MDB_SET);
            if(MDB_SUCCESS == rc) goto COMMIT;

            extensions::graphdb::list::find_head(vtx_set.list_, hint);
            assertm(MDB_SUCCESS == (rc = vtx_set.list_.put(hint, page, extensions::graphdb::flags::put::DEFAULT)), rc);
            assertm(MDB_SUCCESS == (rc = vtx_set.main_.put(iter, hint, extensions::graphdb::flags::put::DEFAULT)), rc);
        }

        {
            extensions::graphdb::Cursor cursor(txn, adj_mtx.list_);

            using iter_t = extensions::graphdb::schema::graph_adj_mtx_key_t;
            using hint_t = extensions::graphdb::schema::list_key_t;
            using value_t = uint8_t;

            iter_t iter = {graph, 0};
            hint_t hint = {0,0};
            std::array<uint8_t, extensions::page_size> page = {0};

            rc = cursor.get(iter, hint, MDB_cursor_op::MDB_SET);
            if(MDB_SUCCESS == rc) goto COMMIT;

            for(size_t i = 0; i < extensions::page_size * extensions::bitarray::cellsize; i++)
            {
                iter.tail() = i;
                hint = {i,0};
                extensions::graphdb::list::find_head(adj_mtx.list_, hint);
                assertm(MDB_SUCCESS == (rc = adj_mtx.list_.put(hint, page, extensions::graphdb::flags::put::DEFAULT)), rc);
                assertm(MDB_SUCCESS == (rc = adj_mtx.main_.put(iter, hint, extensions::graphdb::flags::put::DEFAULT)), rc);
            }
        }
COMMIT:
        assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
    }

private:
    int load_vtx_set_page(extensions::graphdb::schema::DatabaseSet const& parent, vtx_set_key_t& key, iter_t& query)
    {
        int ret = MDB_SUCCESS, rc = 0;

        if(query == vtx_set_iter_ && query.head()) return ret;
        extensions::graphdb::Transaction txn(env_, parent.main_.txn_, extensions::graphdb::flags::txn::WRITE);
        extensions::graphdb::schema::DatabaseSet vtx_set(txn, parent);

        if(0 == query.head())
        {
            iter_t head;
            assertm(MDB_SUCCESS == (rc = vtx_set.main_.get(key, head)), rc);  // get head of list
            query.head() = head.head();
        }

        rc = vtx_set.list_.get(query, vtx_set_page_);
        assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);

        if(MDB_SUCCESS == rc)
        {
            vtx_set_acc_ = vtx_set_page_.accessor<bitarray::cell_t, 1UL>();
            vtx_set_iter_ = query;
        }

        ret = rc;

        assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
        return ret;
    }
    int load_adj_mtx_page(extensions::graphdb::schema::DatabaseSet const& parent, adj_mtx_key_t& key, iter_t& query)
    {
        int ret = MDB_SUCCESS, rc = 0;

        if(key == adj_mtx_key_ && query == adj_mtx_iter_ && query.head()) return ret;
        extensions::graphdb::Transaction txn(env_, parent.main_.txn_, extensions::graphdb::flags::txn::WRITE);
        extensions::graphdb::schema::DatabaseSet adj_mtx(txn, parent);

        if(key.vertex() != adj_mtx_key_.vertex() || 0 == query.head())
        {
            iter_t head;
            assertm(MDB_SUCCESS == (rc = adj_mtx.main_.get(key, head)), rc, key.graph(), key.vertex());  // get head of list
            query.head() = head.head();
        }

        rc = adj_mtx.list_.get(query, adj_mtx_page_);
        assertm(extensions::iter::in(std::array<int,2>{MDB_SUCCESS, MDB_NOTFOUND}, rc), rc);

        if(MDB_SUCCESS == rc)
        {
            adj_mtx_acc_ = adj_mtx_page_.accessor<bitarray::cell_t, 1UL>();
            adj_mtx_key_ = key;
            adj_mtx_iter_ = query;
        }

        ret = rc;

        assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
        return ret;
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
    graphdb::Environment& env_;
    torch::TensorOptions options_ = torch::TensorOptions().dtype(torch::kUInt8)
                                                          .requires_grad(false);
    torch::Tensor vtx_set_page_ = torch::zeros(extensions::page_size, options_);
    torch::TensorAccessor<bitarray::cell_t, 1UL>
                  vtx_set_acc_ = vtx_set_page_.accessor<bitarray::cell_t, 1UL>();
    vtx_set_key_t vtx_set_key_;  // holds the graph index
    iter_t vtx_set_iter_;  // points to a page from the bitset of the vertex set
    torch::Tensor adj_mtx_page_ = torch::zeros(extensions::page_size, options_);
    torch::TensorAccessor<bitarray::cell_t, 1UL>
                  adj_mtx_acc_ = adj_mtx_page_.accessor<bitarray::cell_t, 1UL>();
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

    extensions::iter::GeneratedEnumerable<feature::index_t> vertices()
    {
        auto self = static_cast<parent_t*>(this);
        if(std::holds_alternative<Memory>(*self))
            return std::get_if<Memory>(self)->vertices();
        if(std::holds_alternative<Database>(*self))
            return std::get_if<Database>(self)->vertices();
    }
    extensions::iter::GeneratedEnumerable<std::pair<feature::index_t, feature::index_t>> edges()
    {
        auto self = static_cast<parent_t*>(this);
        if(std::holds_alternative<Memory>(*self))
            return std::get_if<Memory>(self)->edges();
        if(std::holds_alternative<Database>(*self))
            return std::get_if<Database>(self)->edges();
    }

    template<typename T>
    storage_t(T value)
    : parent_t(std::move(value))
    {}
};

using storage_ptr_t = ptr_t<storage_t>;

}}}  // namespace extensions::graph::storage

#endif  // EXTENSIONS_GRAPH_STORAGE_H
