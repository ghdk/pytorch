#include "../system.h"
#include "../iter/iter.h"
#include "../bitarray/bitarray.h"
#include "../graph/feature.h"

extern "C"
{
#include "lmdb.h"
}

#include "db.h"
#include "schema.h"
#include "envpool.h"
#include "graphdb.h"

// The IDE doesn't like the PYBIND11_MODULE macro, hence we redirect it
// to this function which is easier to read.
void PYBIND11_MODULE_IMPL(py::module_ m)
{
    m.attr("PAGE_SIZE") = pybind11::int_(extensions::graphdb::schema::page_size);

    auto c = py::class_<extensions::graphdb::schema::TransactionNode, extensions::ptr_t<extensions::graphdb::schema::TransactionNode>>(m, "TransactionNode");
    m.def("make_transaction_node", +[](std::string filename)
          {
            extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);
            return std::make_shared<extensions::graphdb::schema::TransactionNode>(std::ref(env), extensions::graphdb::flags::txn::NESTED_RW);
          });

    /**
     * Since the key_t is formed by the size of the key, in Python we cannot
     * expose all the key type aliases. Instead we have one alias per key size.
     */

    auto pyks = py::class_<extensions::graphdb::schema::single_key_t, extensions::ptr_t<extensions::graphdb::schema::single_key_t>>(m, "__single_key__");
    auto pykd = py::class_<extensions::graphdb::schema::double_key_t, extensions::ptr_t<extensions::graphdb::schema::double_key_t>>(m, "__double_key__");
    auto pykt = py::class_<extensions::graphdb::schema::triple_key_t, extensions::ptr_t<extensions::graphdb::schema::triple_key_t>>(m, "__triple_key__");

    m.def("make_list_key"          , +[](typename extensions::graphdb::schema::double_key_t::value_type a,
                                         typename extensions::graphdb::schema::double_key_t::value_type b)
                                         { return extensions::graphdb::schema::double_key_t({a, b}); });
    m.def("make_vertex_feature_key", +[](typename extensions::graphdb::schema::triple_key_t::value_type a,
                                         typename extensions::graphdb::schema::triple_key_t::value_type b,
                                         typename extensions::graphdb::schema::triple_key_t::value_type c)
                                         { return extensions::graphdb::schema::triple_key_t({a, b, c}); });
    m.def("make_edge_feature_key"  , +[](typename extensions::graphdb::schema::triple_key_t::value_type a,
                                         typename extensions::graphdb::schema::triple_key_t::value_type b,
                                         typename extensions::graphdb::schema::triple_key_t::value_type c)
                                         { return extensions::graphdb::schema::triple_key_t({a, b, c}); });
    m.def("make_graph_feature_key" , +[](typename extensions::graphdb::schema::double_key_t::value_type a,
                                         typename extensions::graphdb::schema::double_key_t::value_type b)
                                         { return extensions::graphdb::schema::double_key_t({a, b}); });
    m.def("make_graph_adj_mtx_key" , +[](typename extensions::graphdb::schema::double_key_t::value_type a,
                                         typename extensions::graphdb::schema::double_key_t::value_type b)
                                         { return extensions::graphdb::schema::double_key_t({a, b}); });
    m.def("make_graph_vtx_set_key" , +[](typename extensions::graphdb::schema::single_key_t::value_type a)
                                         { return extensions::graphdb::schema::single_key_t({a}); });

    m.def("view_list_key"          , +[](extensions::graphdb::schema::double_key_t key){ return std::tuple{key.data()[0], key.data()[1]}; });
    m.def("view_vertex_feature_key", +[](extensions::graphdb::schema::triple_key_t key){ return std::tuple{key.data()[0], key.data()[1], key.data()[2]}; });
    m.def("view_edge_feature_key"  , +[](extensions::graphdb::schema::triple_key_t key){ return std::tuple{key.data()[0], key.data()[1], key.data()[2]}; });
    m.def("view_graph_feature_key" , +[](extensions::graphdb::schema::double_key_t key){ return std::tuple{key.data()[0], key.data()[1]}; });
    m.def("view_graph_adj_mtx_key" , +[](extensions::graphdb::schema::double_key_t key){ return std::tuple{key.data()[0], key.data()[1]}; });
    m.def("view_graph_vtx_set_key" , +[](extensions::graphdb::schema::single_key_t key){ return std::tuple{key.data()[0]}; });

    {
        auto mh = m.def_submodule("hash", "");

        {
            auto ms = mh.def_submodule("vertex");

            ms.def("visit", +[](extensions::ptr_t<extensions::graphdb::schema::TransactionNode> txn,
                                py::bytes data,
                                extensions::graphdb::hash::visitor_t<extensions::graphdb::schema::vertex_feature_key_t> pyfunc)
                   {
                        std::string_view data_v(data);
                        extensions::graphdb::hash::visit(txn->vertex_, data_v, pyfunc);
                   });
        }

        {
            auto ms = mh.def_submodule("edge");

            ms.def("visit", +[](extensions::ptr_t<extensions::graphdb::schema::TransactionNode> txn,
                                py::bytes data,
                                extensions::graphdb::hash::visitor_t<extensions::graphdb::schema::edge_feature_key_t> pyfunc)
                   {
                        std::string_view data_v(data);
                        extensions::graphdb::hash::visit(txn->edge_, data_v, pyfunc);
                   });
        }

        {
            auto ms = mh.def_submodule("graph");

            ms.def("visit", +[](extensions::ptr_t<extensions::graphdb::schema::TransactionNode> txn,
                                py::bytes data,
                                extensions::graphdb::hash::visitor_t<extensions::graphdb::schema::graph_feature_key_t> pyfunc)
                   {
                        std::string_view data_v(data);
                        extensions::graphdb::hash::visit(txn->graph_, data_v, pyfunc);
                   });
        }
    }  // submodule hash

    {
        auto mf = m.def_submodule("feature", "");
        using visitor_t = std::function<int(py::bytes)>;

        {
            auto mv = mf.def_submodule("vertex");

            mv.def("visit", +[](extensions::ptr_t<extensions::graphdb::schema::TransactionNode> txn, extensions::graphdb::schema::vertex_feature_key_t key, visitor_t pyfunc)
                   {
                        // We wrap the Python function into a lambda that will
                        // translate the key and size into a bytes object.
                        auto visitor = [&](extensions::graphdb::schema::list_key_t iter,  size_t sz){
                            std::string ret(sz, '\0');
                            extensions::graphdb::list::read(txn->vertex_.list_, iter, ret);
                            return pyfunc(py::bytes(ret));
                        };

                        extensions::graphdb::feature::visit(txn->vertex_, key, visitor);
                   });
        }

        {
            auto mv = mf.def_submodule("edge");

            mv.def("visit", +[](extensions::ptr_t<extensions::graphdb::schema::TransactionNode> txn, extensions::graphdb::schema::edge_feature_key_t key, visitor_t pyfunc)
                   {
                        // We wrap the Python function into a lambda that will
                        // translate the key and size into a bytes object.
                        auto visitor = [&](extensions::graphdb::schema::list_key_t iter,  size_t sz){
                            std::string ret(sz, '\0');
                            extensions::graphdb::list::read(txn->edge_.list_, iter, ret);
                            return pyfunc(py::bytes(ret));
                        };

                        extensions::graphdb::feature::visit(txn->edge_, key, visitor);
                   });
        }

        {
            auto mv = mf.def_submodule("graph");

            mv.def("visit", +[](extensions::ptr_t<extensions::graphdb::schema::TransactionNode> txn, extensions::graphdb::schema::graph_feature_key_t key, visitor_t pyfunc)
                   {
                        // We wrap the Python function into a lambda that will
                        // translate the key and size into a bytes object.
                        auto visitor = [&](extensions::graphdb::schema::list_key_t iter,  size_t sz){
                            std::string ret(sz, '\0');
                            extensions::graphdb::list::read(txn->graph_.list_, iter, ret);
                            return pyfunc(py::bytes(ret));
                        };

                        extensions::graphdb::feature::visit(txn->graph_, key, visitor);
                   });
        }
    }  // submodule feature

    {
        /**
         * Test submodule
         */

        auto mt = m.def_submodule("test", "");

        mt.attr("SCHEMA") = py::cast(extensions::graphdb::schema::SCHEMA);
        mt.attr("VERTEX_FEATURE")     = py::cast(extensions::graphdb::schema::VERTEX_FEATURE);
        mt.attr("VERTEX_FEATURE_H")   = py::cast(extensions::graphdb::schema::VERTEX_FEATURE_H);
        mt.attr("VERTEX_FEATURE_L")   = py::cast(extensions::graphdb::schema::VERTEX_FEATURE_L);
        mt.attr("EDGE_FEATURE")       = py::cast(extensions::graphdb::schema::EDGE_FEATURE);
        mt.attr("EDGE_FEATURE_H")     = py::cast(extensions::graphdb::schema::EDGE_FEATURE_H);
        mt.attr("EDGE_FEATURE_L")     = py::cast(extensions::graphdb::schema::EDGE_FEATURE_L);
        mt.attr("GRAPH_FEATURE")      = py::cast(extensions::graphdb::schema::GRAPH_FEATURE);
        mt.attr("GRAPH_FEATURE_H")    = py::cast(extensions::graphdb::schema::GRAPH_FEATURE_H);
        mt.attr("GRAPH_FEATURE_L")    = py::cast(extensions::graphdb::schema::GRAPH_FEATURE_L);
        mt.attr("ADJACENCY_MATRIX")   = py::cast(extensions::graphdb::schema::ADJACENCY_MATRIX);
        mt.attr("ADJACENCY_MATRIX_H") = py::cast(extensions::graphdb::schema::ADJACENCY_MATRIX_H);
        mt.attr("ADJACENCY_MATRIX_L") = py::cast(extensions::graphdb::schema::ADJACENCY_MATRIX_L);
        mt.attr("VERTEX_SET")         = py::cast(extensions::graphdb::schema::VERTEX_SET);
        mt.attr("VERTEX_SET_H")       = py::cast(extensions::graphdb::schema::VERTEX_SET_H);
        mt.attr("VERTEX_SET_L")       = py::cast(extensions::graphdb::schema::VERTEX_SET_L);


        mt.def("test_graphdb_cardinalities",
               +[]{

                   std::string filename = "./test.db";
                   int rc = 0;

                   extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);
                   assertm(0 == env.cardinality(), "cardinality=", env.cardinality());

                   {
                       extensions::graphdb::Transaction txn(env, extensions::graphdb::flags::txn::WRITE);
                       extensions::graphdb::Database dbA(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::WRITE);
                       assertm(0 == dbA.cardinality(), "cardinality=", dbA.cardinality());

                       {
                           const extensions::graphdb::schema::list_key_t key = {dbA.cardinality(),0};
                           const size_t value = 0xba5eba11;

                           int rc = dbA.put(key, value, 0);
                           assert(MDB_SUCCESS == rc);
                       }

                       extensions::graphdb::Database dbB(txn, env.schema_[extensions::graphdb::schema::EDGE_FEATURE], extensions::graphdb::flags::db::WRITE);
                       assertm(0 == dbB.cardinality(), "cardinality=", dbB.cardinality());

                       {
                           const extensions::graphdb::schema::list_key_t key = {dbB.cardinality(),0};
                           const size_t value = 0x00ddba11;

                           int rc = dbB.put(key, value, 0);
                           assert(MDB_SUCCESS == rc);
                       }

                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   // NB. cardinality reflects the contents of the DB after the
                   // transaction has been committed.
                   assertm(2 == env.cardinality(), "cardinality=", env.cardinality());

                   {
                       extensions::graphdb::Transaction txn(env, extensions::graphdb::flags::txn::READ);
                       extensions::graphdb::Database dbA(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::WRITE);
                       assertm(1 == dbA.cardinality(), "cardinality=", dbA.cardinality());

                       {
                           int rc = 0;
                           extensions::graphdb::schema::list_key_t key = {0,0};
                           size_t value = 0;

                           assertm(MDB_SUCCESS == (rc = dbA.get(key, value)), rc);
                           assertm(0xba5eba11 == value, value);

                       }

                       extensions::graphdb::Database dbB(txn, env.schema_[extensions::graphdb::schema::EDGE_FEATURE], extensions::graphdb::flags::db::WRITE);
                       assertm(1 == dbB.cardinality(), "cardinality=", dbB.cardinality());

                       {
                           int rc = 0;
                           extensions::graphdb::schema::list_key_t key = {0,0};
                           size_t value = 0;

                           assertm(MDB_SUCCESS == (rc = dbB.get(key, value)), rc);
                           assertm(0x00ddba11 == value, value);

                       }

                       assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                   }
                });

        mt.def("test_graphdb_tensor_api",
               +[]{

                   std::string filename = "./test.db";
                   int rc = 0;

                   extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);

                   std::string key_s("tensor");

                   auto orig = torch::rand({1,2,3,5});

                   {
                       auto tensor = orig.detach().clone();
                       tensor = tensor.contiguous();
                       std::vector<float> vector(tensor.data_ptr<float>(), tensor.data_ptr<float>() + tensor.numel());

                       extensions::graphdb::Transaction txn(env, extensions::graphdb::flags::txn::WRITE);
                       extensions::graphdb::Database db(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::WRITE);

                       int rc = db.put(key_s, tensor, 0);
                       assert(MDB_SUCCESS == rc);
                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       extensions::graphdb::Transaction txn(env, extensions::graphdb::flags::txn::READ);
                       extensions::graphdb::Database db(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::READ);
                       auto tensor = torch::zeros(orig.sizes(), orig.options());

                       assertm(MDB_SUCCESS == (rc = db.get(key_s, tensor)), rc);
                       assertm(orig.equal(tensor), orig, tensor);
                       assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                   }
               });

        mt.def("test_graphdb_string_api",
               +[]{

                   std::string filename = "./test.db";
                   int rc = 0;

                   extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);

                   std::string key("key");
                   std::string value("value");
                   {
                       extensions::graphdb::Transaction txn(env, extensions::graphdb::flags::txn::WRITE);
                       extensions::graphdb::Database db(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::WRITE);
                       assertm(MDB_SUCCESS == (rc = db.put(key, value, 0)), rc);
                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       extensions::graphdb::Transaction txn(env, extensions::graphdb::flags::txn::READ);
                       extensions::graphdb::Database db(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::READ);
                       std::string read(value.size(), '\0');

                       assertm(MDB_SUCCESS == (rc = db.get(key, read)), rc);

                       assertm(value == read, value, read);
                       assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                   }
               });

        mt.def("test_graphdb_int_api",
               +[]{

                   std::string filename = "./test.db";
                   int rc = 0;

                   extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);

                   const uint64_t value = 0xba5eba11;
                   std::string key("int");

                   {
                       extensions::graphdb::Transaction txn(env, extensions::graphdb::flags::txn::WRITE);
                       extensions::graphdb::Database db(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::WRITE);
                       rc = db.put(key, value, 0);
                       assert(MDB_SUCCESS == rc);
                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       extensions::graphdb::Transaction txn(env, extensions::graphdb::flags::txn::READ);
                       extensions::graphdb::Database db(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::READ);
                       uint64_t read = 0;
                       assertm(MDB_SUCCESS == (rc = db.get(key, read)), rc);
                       assertm(value == read, read);
                       assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                   }
               });

        mt.def("test_graphdb_find_head",
               +[]{

                   using namespace extensions::graphdb;

                   std::string filename = "./test.db";
                   int rc;

                   extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);

                   {
                       {
                           Transaction txnW(env, flags::txn::WRITE);
                           Database dbW(txnW, env.schema_[schema::L(schema::VERTEX_FEATURE)], flags::db::WRITE);

                           schema::list_key_t hint;
                           schema::list_key_t value;
                           mdb_view<schema::list_key_t> hint_v(&value, 1);

                           list::find_head(dbW, hint);
                           assertm(0 != hint.head(), hint.head(), hint.tail());
                           assertm(MDB_SUCCESS == (rc = txnW.commit()), rc);
                       }

                       {
                           Transaction txnR(env, flags::txn::READ);
                           Database dbR(txnR, env.schema_[schema::L(schema::VERTEX_FEATURE)], flags::db::READ);
                           schema::meta_key_t key = schema::META_KEY_PAGE;
                           schema::meta_page_t value = 0;

                           assertm(MDB_SUCCESS == (rc = dbR.get(key, value)), rc);
                           assertm(1 == value, value);
                           assertm(MDB_SUCCESS == (rc = txnR.abort()), rc);
                       }

                       {
                           // fill in the first page
                           Transaction txnE(env, flags::txn::WRITE);
                           Database dbE(txnE, env.schema_[schema::L(schema::VERTEX_FEATURE)], flags::db::WRITE);

                           schema::list_key_t hint;
                           schema::list_key_t value;
                           for(typename schema::list_key_t::value_type h = 0; h < extensions::graphdb::schema::page_size * CHAR_BIT; ++h)
                           {
                               if(schema::RESERVED == h) continue;
                               hint.head() = h;
                               value.graph() = 0xba5eba11;
                               assertm(MDB_SUCCESS == (rc = dbE.put(hint, value, flags::put::DEFAULT)), rc);
                           }
                           assertm(MDB_SUCCESS == (rc = txnE.commit()), rc);
                       }

                       {
                           // trigger expansion
                           Transaction txn(env, flags::txn::WRITE);
                           Database db(txn, env.schema_[schema::L(schema::VERTEX_FEATURE)], flags::db::WRITE);

                           schema::list_key_t hint;
                           schema::list_key_t value;
                           mdb_view<schema::list_key_t> hint_v(&value, 1);

                           list::find_head(db, hint);
                           assertm(extensions::graphdb::schema::page_size * CHAR_BIT <= hint.head(), hint.head(), hint.tail());
                           assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                       }

                       {
                           Transaction txn(env, flags::txn::READ);
                           Database db(txn, env.schema_[schema::L(schema::VERTEX_FEATURE)], flags::db::READ);
                           schema::meta_key_t key = schema::META_KEY_PAGE;
                           schema::meta_page_t value = 0;

                           assertm(MDB_SUCCESS == (rc = db.get(key, value)), rc);
                           assertm(2 == value, value);
                           assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                       }
                   }

               });

        mt.def("test_graphdb_key_type",
               +[]{
                       {
                            using key_t = extensions::graphdb::schema::graph_vtx_set_key_t;
                            key_t key;
                            key.graph() = 0xba5eba11;
                            assertm(*(key.data()) == 0xba5eba11, *(key.data()));
                       }

                       {
                            using key_t = extensions::graphdb::schema::graph_adj_mtx_key_t;

                            key_t key;
                            key.graph() = 0xba5eba11;
                            assertm(*(key.data()) == 0xba5eba11, *(key.data()));

                            key.vertex() = 0xc0011ad5;
                            assertm(*(key.data() + 1) == 0xc0011ad5, *(key.data() + 1));

                            key.attribute() = 0xdeadbeef;
                            assertm(*(key.data() + 1) == 0xdeadbeef, *(key.data() + 1));
                       }

                       {
                           using key_t = extensions::graphdb::schema::vertex_feature_key_t;

                           key_t key;
                           key.graph() = 0xba5eba11;
                           assertm(*(key.data()) == 0xba5eba11, *(key.data()));

                           key.vertex() = 0xc0011ad5;
                           assertm(*(key.data() + 1) == 0xc0011ad5, *(key.data() + 1));

                           key.edge() = 0xfa5757af;
                           assertm(*(key.data() + 1) == 0xfa5757af, *(key.data() + 1));

                           key.attribute() = 0xdeadbeef;
                           assertm(*(key.data() + 2) == 0xdeadbeef, *(key.data() + 2));
                       }
               });

        mt.def("test_graphdb_list_write_and_read",
               +[]{
                   using namespace extensions::graphdb;

                   std::string filename = "./test.db";
                   int rc;

                   extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);

                   using elem_t = uint16_t;
                   using buff_t = std::array<elem_t, extensions::graphdb::schema::page_size>;  // == 2 pages
                   size_t chunk = extensions::graphdb::schema::page_size / sizeof(elem_t);

                   {
                       buff_t buffer{0};
                       buffer[0] = 0xba5e;
                       buffer[chunk-1] = 0xba11;
                       buffer[chunk] = 0xc001;
                       buffer[2*chunk-1] = 0xbee5;

                       Transaction txn(env, flags::txn::WRITE);
                       Database db(txn, env.schema_[schema::H(schema::VERTEX_FEATURE)], flags::db::WRITE);

                       schema::list_key_t head{1,0};

                       list::write(db, head, buffer);

                       // verify the DB

                       schema::list_key_t iter;
                       buff_t read;

                       iter = schema::list_key_t{1,0};
                       assertm(MDB_SUCCESS == (rc = db.get(iter, read)), rc);

                       assertm(0xba5e == read[0],             read[0]);
                       assertm(0      == read[0 + 1],         read[0 + 1]);
                       assertm(0      == read[0 + chunk - 2], read[0 + chunk - 2]);
                       assertm(0xba11 == read[0 + chunk - 1], read[0 + chunk - 1]);

                       iter = schema::list_key_t{1,1};
                       assertm(MDB_SUCCESS == (rc = db.get(iter, read)), rc);

                       assertm(0xc001 == read[0],             read[0]);
                       assertm(0      == read[0 + 1],         read[0 + 1]);
                       assertm(0      == read[0 + chunk - 2], read[0 + chunk - 2]);
                       assertm(0xbee5 == read[0 + chunk - 1], read[0 + chunk - 1]);

                       iter = schema::list_key_t{1,2};
                       assertm(MDB_NOTFOUND == (rc = db.get(iter, read)), rc);

                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       Transaction txn(env, flags::txn::NESTED_RO);
                       Database db(txn, env.schema_[schema::H(schema::VERTEX_FEATURE)], flags::db::READ);

                       buff_t buffer{0};
                       schema::list_key_t head{1,0};

                       list::read(db, head, buffer);

                       assertm(0xba5e == buffer[0], buffer[0]);
                       assertm(0      == buffer[1], buffer[1]);
                       assertm(0      == buffer[chunk - 2], buffer[chunk - 2]);
                       assertm(0xba11 == buffer[chunk - 1], buffer[chunk - 1]);
                       assertm(0xc001 == buffer[chunk],     buffer[chunk]);
                       assertm(0      == buffer[chunk + 1], buffer[chunk + 1]);
                       assertm(0      == buffer[chunk * 2 - 2], buffer[chunk * 2 - 2]);
                       assertm(0xbee5 == buffer[chunk * 2 - 1], buffer[chunk * 2 - 1]);

                       assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                   }

               });

        mt.def("test_graphdb_list_write_and_clear",
               +[]{
                   using namespace extensions::graphdb;

                   std::string filename = "./test.db";
                   int rc;

                   extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);

                   using elem_t = uint16_t;
                   using buff_t = std::array<elem_t, extensions::graphdb::schema::page_size>;  // == 2 pages
                   size_t chunk = extensions::graphdb::schema::page_size / sizeof(elem_t);

                   {
                       buff_t buffer{0};
                       buffer[0] = 0xba5e;
                       buffer[chunk-1] = 0xba11;
                       buffer[chunk] = 0xc001;
                       buffer[2*chunk-1] = 0xbee5;

                       Transaction txn(env, flags::txn::WRITE);
                       Database db(txn, env.schema_[schema::H(schema::VERTEX_FEATURE)], flags::db::WRITE);

                       schema::list_key_t head{1,0};

                       list::write(db, head, buffer);

                       // verify the DB

                       schema::list_key_t iter;
                       buff_t read;

                       iter = schema::list_key_t{1,0};
                       assertm(MDB_SUCCESS == (rc = db.get(iter, read)), rc);

                       iter = schema::list_key_t{1,1};
                       assertm(MDB_SUCCESS == (rc = db.get(iter, read)), rc);

                       iter = schema::list_key_t{1,2};
                       assertm(MDB_NOTFOUND == (rc = db.get(iter, read)), rc);

                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       schema::list_key_t head{1,0};

                       Transaction txn(env, flags::txn::WRITE);
                       Database db(txn, env.schema_[schema::H(schema::VERTEX_FEATURE)], flags::db::WRITE);

                       list::clear(db, head);

                       // verify the DB

                       schema::list_key_t iter;
                       buff_t read;

                       iter = schema::list_key_t{1,0};
                       assertm(MDB_NOTFOUND == (rc = db.get(iter, read)), rc);

                       iter = schema::list_key_t{1,1};
                       assertm(MDB_NOTFOUND == (rc = db.get(iter, read)), rc);

                       iter = schema::list_key_t{1,2};
                       assertm(MDB_NOTFOUND == (rc = db.get(iter, read)), rc);

                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

               });

        mt.def("test_graphdb_hash_write",
               +[]{
                   using namespace extensions::graphdb;

                   std::string filename = "./test.db";
                   int rc;

                   extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);

                   {
                       Transaction txn(env, flags::txn::WRITE);
                       extensions::graphdb::schema::DatabaseSet db(txn, extensions::graphdb::schema::VERTEX_FEATURE, extensions::graphdb::flags::db::WRITE);

                       schema::list_key_t hash{1,0};
                       schema::list_key_t value;
                       schema::list_key_t iter = hash;
                       size_t sz;

                       {
                           list::size(db.hash_, iter, sz);
                           assertm(0 == iter.tail(), iter.tail());
                       }

                       value = {0xba5e, 0xba11};
                       hash::write(db, hash, value);

                       {
                           list::size(db.hash_, iter, sz);
                           assertm(1 == iter.tail(), iter.tail());
                       }

                       value = {0xdead, 0xbeef};
                       hash::write(db, hash, value);

                       {
                           list::size(db.hash_, iter, sz);
                           assertm(2 == iter.tail(), iter.tail());
                       }

                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       Transaction txn(env, flags::txn::READ);
                       extensions::graphdb::schema::DatabaseSet db(txn, extensions::graphdb::schema::VERTEX_FEATURE, extensions::graphdb::flags::db::READ);

                       schema::list_key_t hash;
                       schema::list_key_t value;

                       hash = {1,0};

                       assertm(MDB_SUCCESS == (rc = db.hash_.get(hash, value)), rc);
                       assertm(0xba5e == value.head(), value.head());
                       assertm(0xba11 == value.tail(), value.tail());

                       hash = {1,1};

                       assertm(MDB_SUCCESS == (rc = db.hash_.get(hash, value)), rc);
                       assertm(0xdead == value.head(), value.head());
                       assertm(0xbeef == value.tail(), value.tail());

                       assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                   }
               });

        mt.def("test_graphdb_hash_remove",
               +[]{
                   using namespace extensions::graphdb;

                   std::string filename = "./test.db";
                   int rc;

                   extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);

                   {
                       Transaction txn(env, flags::txn::NESTED_RW);
                       extensions::graphdb::schema::DatabaseSet db(txn, extensions::graphdb::schema::VERTEX_FEATURE, extensions::graphdb::flags::db::WRITE);

                       schema::list_key_t hash{1,0};
                       schema::list_key_t value;

                       value = {0xba5e, 0xba11};
                       hash::write(db, hash, value);

                       value = {0xdead, 0xbeef};
                       hash::write(db, hash, value);

                       value = {0xc001, 0xbee5};
                       hash::write(db, hash, value);

                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       Transaction txn(env, flags::txn::READ);
                       extensions::graphdb::schema::DatabaseSet db(txn, extensions::graphdb::schema::VERTEX_FEATURE, extensions::graphdb::flags::db::WRITE);

                       schema::list_key_t hash;
                       schema::list_key_t value;

                       hash = {1,0};
                       assertm(MDB_SUCCESS == (rc = db.hash_.get(hash, value)), rc);

                       hash = {1,1};
                       assertm(MDB_SUCCESS == (rc = db.hash_.get(hash, value)), rc);

                       hash = {1,2};
                       assertm(MDB_SUCCESS == (rc = db.hash_.get(hash, value)), rc);

                       hash = {1,3};
                       assertm(MDB_NOTFOUND == (rc = db.hash_.get(hash, value)), rc);

                       assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                   }

                   {
                       Transaction txn(env, flags::txn::NESTED_RW);
                       extensions::graphdb::schema::DatabaseSet db(txn, extensions::graphdb::schema::VERTEX_FEATURE, extensions::graphdb::flags::db::WRITE);

                       {
                           schema::list_key_t hash;

                           hash = {1,0};
                           list::remove(db.hash_, hash);
                       }

                       {
                           schema::list_key_t iter;
                           schema::list_key_t value;

                           iter = {1,1};
                           assertm(MDB_SUCCESS == (rc = db.hash_.get(iter, value)), rc);

                           iter = {1,2};
                           assertm(MDB_NOTFOUND == (rc = db.hash_.get(iter, value)), rc);

                           iter = {1,3};
                           assertm(MDB_NOTFOUND == (rc = db.hash_.get(iter, value)), rc);

                           assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                       }
                   }
               });

        mt.def("test_graphdb_hash_visit",
               +[]{
                   using namespace extensions::graphdb;

                   std::string filename = "./test.db";
                   int rc;

                   extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);

                   {
                       Transaction txn(env, flags::txn::NESTED_RW);
                       extensions::graphdb::schema::DatabaseSet db(txn, extensions::graphdb::schema::VERTEX_FEATURE, extensions::graphdb::flags::db::WRITE);

                       schema::graph_feature_key_t key = {0xba5e, 0xf00};
                       schema::graph_feature_key_t::value_type value = 0xba11;

                       extensions::graphdb::feature::write(db, key, value);
                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       Transaction txn(env, flags::txn::NESTED_RW);
                       extensions::graphdb::schema::DatabaseSet db(txn, extensions::graphdb::schema::VERTEX_FEATURE, extensions::graphdb::flags::db::WRITE);

                       schema::graph_feature_key_t key = {0,0};  // get it from the DB
                       schema::graph_feature_key_t::value_type value = 0xba11;

                       extensions::graphdb::hash::visitor_t<schema::graph_feature_key_t> visitor =
                               [&](schema::graph_feature_key_t key)
                               {
                                   assertm(0xba5e == key.head(), key.head());
                                   assertm(0xf00  == key.tail(), key.tail());
                                   return MDB_SUCCESS;
                               };
                       rc = extensions::graphdb::hash::visit(db, value, visitor);
                       assertm(MDB_SUCCESS == rc, rc);
                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }
               });

        mt.def("test_graphdb_cursor_next",
               +[]{
                   using namespace extensions::graphdb;

                   std::string filename = "./test.db";
                   int rc;

                   extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);

                   {
                       Transaction txn(env, flags::txn::NESTED_RW);
                       extensions::graphdb::schema::DatabaseSet db(txn, extensions::graphdb::schema::VERTEX_FEATURE, extensions::graphdb::flags::db::WRITE);

                       for(size_t head = 1; head < 4; head++)
                       {
                           schema::list_key_t hash{head,0};
                           schema::list_key_t value;

                           value = {0xba5e, 0xba11};
                           hash::write(db, hash, value);

                           value = {0xdead, 0xbeef};
                           hash::write(db, hash, value);

                           value = {0xc001, 0xbee5};
                           hash::write(db, hash, value);
                       }

                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       Transaction txn(env, flags::txn::READ);
                       extensions::graphdb::schema::DatabaseSet db(txn, extensions::graphdb::schema::VERTEX_FEATURE, extensions::graphdb::flags::db::WRITE);

                       {
                           Cursor cursor(txn, db.hash_);

                           size_t head = 2;
                           schema::list_key_t iter{head,0};
                           schema::list_key_t value;

                           assertm(MDB_SUCCESS == (rc = cursor.get(iter, value, MDB_cursor_op::MDB_SET)), rc);

                           assertm(MDB_SUCCESS == (rc = cursor.get(iter, value, MDB_cursor_op::MDB_NEXT)), rc);
                           assertm(head == iter.head(), iter.head());
                           assertm(1 == iter.tail(), iter.tail());
                           assertm(0xdead == value.head(), value.head());
                           assertm(0xbeef == value.tail(), value.tail());

                           assertm(MDB_SUCCESS == (rc = cursor.get(iter, value, MDB_cursor_op::MDB_NEXT)), rc);
                           assertm(head == iter.head(), iter.head());
                           assertm(2 == iter.tail(), iter.tail());

                           assertm(MDB_SUCCESS == (rc = cursor.get(iter, value, MDB_cursor_op::MDB_NEXT)), rc);
                           assertm(head+1 == iter.head(), iter.head());  // moves to the next head
                           assertm(0 == iter.tail(), iter.tail());
                       }

                       assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                   }
               });

        mt.def("test_graphdb_feature_write_and_read",
               +[]{
                   using namespace extensions::graphdb;

                   std::string filename = "./test.db";
                   int rc;

                   extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);

                   using elem_t = uint16_t;
                   using buff_t = std::array<elem_t, extensions::graphdb::schema::page_size>;  // == 2 pages
                   size_t chunk = extensions::graphdb::schema::page_size / sizeof(elem_t);

                   {
                       buff_t buffer{0};
                       buffer[0] = 0xba5e;
                       buffer[chunk-1] = 0xba11;
                       buffer[chunk] = 0xc001;
                       buffer[2*chunk-1] = 0xbee5;

                       Transaction txn(env, flags::txn::NESTED_RW);
                       extensions::graphdb::schema::DatabaseSet db(txn, extensions::graphdb::schema::VERTEX_FEATURE, extensions::graphdb::flags::db::WRITE);
                       extensions::graphdb::schema::vertex_feature_key_t key = {0,0,0};
                       extensions::graphdb::feature::write(db, key, buffer);
                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       Transaction txn(env, flags::txn::NESTED_RW);
                       extensions::graphdb::schema::DatabaseSet db(txn, extensions::graphdb::schema::VERTEX_FEATURE, extensions::graphdb::flags::db::WRITE);

                       buff_t buffer{0};
                       extensions::graphdb::schema::vertex_feature_key_t key = {0,0,0};
                       extensions::graphdb::feature::visit(db, key,
                                                           [&](schema::list_key_t head, size_t size)
                                                           {
                                                               extensions::graphdb::list::read(db.list_, head, buffer);
                                                               return MDB_SUCCESS;
                                                           });


                       assertm(0xba5e == buffer[0], buffer[0]);
                       assertm(0      == buffer[1], buffer[1]);
                       assertm(0      == buffer[chunk - 2], buffer[chunk - 2]);
                       assertm(0xba11 == buffer[chunk - 1], buffer[chunk - 1]);
                       assertm(0xc001 == buffer[chunk],     buffer[chunk]);
                       assertm(0      == buffer[chunk + 1], buffer[chunk + 1]);
                       assertm(0      == buffer[chunk * 2 - 2], buffer[chunk * 2 - 2]);
                       assertm(0xbee5 == buffer[chunk * 2 - 1], buffer[chunk * 2 - 1]);

                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

               });

        mt.def("test_graphdb_transaction_node",
               +[]{

                   std::string filename = "./test.db";
                   int rc = 0;

                   extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);
                   extensions::graphdb::schema::TransactionNode root{env, extensions::graphdb::flags::txn::WRITE};

                   const uint64_t value = 0xba5eba11;
                   std::string key("int");

                   {
                       extensions::graphdb::schema::TransactionNode child{root, extensions::graphdb::flags::txn::NESTED_RW};
                       assert(MDB_SUCCESS == (rc = child.vertex_.main_.put(key, value, 0)));
                   }

                   {
                       extensions::graphdb::schema::TransactionNode child{root, extensions::graphdb::flags::txn::NESTED_RO};
                       uint64_t read = 0;
                       assertm(MDB_SUCCESS == (rc = child.vertex_.main_.get(key, read)), rc);
                       assertm(value == read, read);
                   }
               });

        mt.def("test_graphdb_transaction_api",
               +[]{

                   std::string filename = "./test.db";
                   int rc = 0;

                   extensions::graphdb::Environment& env = extensions::graphdb::EnvironmentPool::environment(filename);
                   extensions::graphdb::schema::TransactionNode root{env, extensions::graphdb::flags::txn::WRITE};

                   using elem_t = uint8_t;
                   using buff_t = std::array<elem_t, extensions::graphdb::schema::page_size>;
                   size_t chunk = extensions::graphdb::schema::page_size / sizeof(elem_t);

                   std::string key("buffer");

                   {
                       extensions::graphdb::schema::TransactionNode child{root, extensions::graphdb::flags::txn::NESTED_RW};

                       buff_t value{0};
                       assert(MDB_SUCCESS == (rc = child.vertex_.main_.put(key, value, 0)));
                       // modifying the buffer after the write does not affect the value in the DB
                       value[0] = 1;
                   }

                   {
                       extensions::graphdb::schema::TransactionNode child{root, extensions::graphdb::flags::txn::NESTED_RO};
                       buff_t value{0};
                       assertm(MDB_SUCCESS == (rc = child.vertex_.main_.get(key, value)), rc);
                       assertm(0 == value[0], value[0]);
                   }
               });
    }
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    PYBIND11_MODULE_IMPL(m);
}
