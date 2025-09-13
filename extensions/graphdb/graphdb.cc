#include "../system.h"
#include "../iter/iter.h"
#include "../graph/feature.h"

extern "C"
{
#include "lmdb.h"
}

#include "schema.h"
#include "db.h"
#include "graphdb.h"

// The IDE doesn't like the PYBIND11_MODULE macro, hence we redirect it
// to this function which is easier to read.
void PYBIND11_MODULE_IMPL(py::module_ m)
{
    {
        /**
         * Test submodule
         */

        auto mt = m.def_submodule("test", "");

        mt.def("test_graphdb_cardinalities",
               +[]{

                   std::string filename = "./test.db";
                   int rc = 0;

                   extensions::graphdb::Environment env(filename, extensions::graphdb::schema::SCHEMA);
                   assertm(0 == env.cardinality(), "cardinality=", env.cardinality());

                   {
                       extensions::graphdb::Transaction txn(env, 0);
                       extensions::graphdb::Database dbA(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::WRITE);
                       assertm(0 == dbA.cardinality(), "cardinality=", dbA.cardinality());

                       {
                           const size_t key = dbA.cardinality();
                           const size_t value = 0xba5eba11;

                           extensions::graphdb::mdb_view keyv(&key, 1);
                           extensions::graphdb::mdb_view valuev(&value, 1);

                           int rc = dbA.put(keyv, valuev, 0);
                           assert(MDB_SUCCESS == rc);
                       }

                       extensions::graphdb::Database dbB(txn, env.schema_[extensions::graphdb::schema::EDGE_FEATURE], extensions::graphdb::flags::db::WRITE);
                       assertm(0 == dbB.cardinality(), "cardinality=", dbB.cardinality());

                       {
                           const size_t key = dbB.cardinality();
                           const size_t value = 0x00ddba11;

                           extensions::graphdb::mdb_view keyv(&key, 1);
                           extensions::graphdb::mdb_view valuev(&value, 1);

                           int rc = dbB.put(keyv, valuev, 0);
                           assert(MDB_SUCCESS == rc);
                       }

                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   // NB. cardinality reflects the contents of the DB after the
                   // transaction has been committed.
                   assertm(2 == env.cardinality(), "cardinality=", env.cardinality());

                   {
                       extensions::graphdb::Transaction txn(env, 0);
                       extensions::graphdb::Database dbA(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::WRITE);
                       assertm(1 == dbA.cardinality(), "cardinality=", dbA.cardinality());

                       {
                           size_t key = 0;

                           extensions::graphdb::mdb_view keyv(&key, 1);
                           extensions::graphdb::mdb_view<size_t> value;

                           int rc = dbA.get(keyv, value);
                           assert(MDB_SUCCESS == rc);

                           assert(0xba5eba11 == *(value.begin()));

                       }

                       extensions::graphdb::Database dbB(txn, env.schema_[extensions::graphdb::schema::EDGE_FEATURE], extensions::graphdb::flags::db::WRITE);
                       assertm(1 == dbB.cardinality(), "cardinality=", dbB.cardinality());

                       {
                           size_t key = 0;

                           extensions::graphdb::mdb_view keyv(&key, 1);
                           extensions::graphdb::mdb_view<size_t> value;

                           int rc = dbB.get(keyv, value);
                           assert(MDB_SUCCESS == rc);

                           assert(0x00ddba11 == *(value.begin()));

                       }

                       assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                   }
                });

        mt.def("test_graphdb_tensor_api",
               +[]{

                   std::string filename = "./test.db";
                   int rc = 0;

                   extensions::graphdb::Environment env(filename, extensions::graphdb::schema::SCHEMA);

                   std::string key_s("tensor");
                   extensions::graphdb::mdb_view key(key_s.data(), key_s.size());

                   auto orig = torch::rand({1,2,3,5});

                   {
                       auto tensor = orig.detach().clone();
                       tensor = tensor.contiguous();
                       std::vector<float> vector(tensor.data_ptr<float>(), tensor.data_ptr<float>() + tensor.numel());

                       extensions::graphdb::Transaction txn(env, 0);
                       extensions::graphdb::Database db(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::WRITE);

                       extensions::graphdb::mdb_view value(tensor.data_ptr<float>(), tensor.numel());

                       int rc = db.put(key, value, 0);
                       assert(MDB_SUCCESS == rc);
                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       extensions::graphdb::Transaction txn(env, MDB_RDONLY);
                       extensions::graphdb::Database db(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::READ);
                       extensions::graphdb::mdb_view<float> value;
                       assertm(MDB_SUCCESS == (rc = db.get(key, value)), rc);

                       auto options = torch::TensorOptions().dtype(torch::kFloat);
                       torch::Tensor tensor = torch::from_blob(value.begin(), {1,2,3,5}, options);
                       assertm(orig.equal(tensor), orig, tensor);
                       assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                   }
               });

        mt.def("test_graphdb_string_api",
               +[]{

                   std::string filename = "./test.db";
                   int rc = 0;

                   extensions::graphdb::Environment env(filename, extensions::graphdb::schema::SCHEMA);

                   std::string key("key");
                   std::string value("value");
                   extensions::graphdb::mdb_view key_v(key.data(), key.size());
                   extensions::graphdb::mdb_view val_v(value.data(), value.size());

                   {
                       extensions::graphdb::Transaction txn(env, 0);
                       extensions::graphdb::Database db(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::WRITE);
                       assertm(MDB_SUCCESS == (rc = db.put(key_v, val_v, 0)), rc);
                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       extensions::graphdb::Transaction txn(env, MDB_RDONLY);
                       extensions::graphdb::Database db(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::READ);
                       extensions::graphdb::mdb_view<char> data;

                       assertm(MDB_SUCCESS == (rc = db.get(key_v, data)), rc);

                       std::string read(data.data(), data.size());
                       assertm(value == read, value, read);

                       assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                   }
               });

        mt.def("test_graphdb_int_api",
               +[]{

                   std::string filename = "./test.db";
                   int rc = 0;

                   extensions::graphdb::Environment env(filename, extensions::graphdb::schema::SCHEMA);

                   const uint64_t value = 0xba5eba11;
                   std::string key_s("int");
                   extensions::graphdb::mdb_view key(key_s.data(), key_s.size());

                   {
                       extensions::graphdb::Transaction txn(env, extensions::graphdb::flags::txn::WRITE);
                       extensions::graphdb::Database db(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::WRITE);
                       extensions::graphdb::mdb_view mdbvA(&value, 1);
                       rc = db.put(key, mdbvA, 0);
                       assert(MDB_SUCCESS == rc);
                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       extensions::graphdb::Transaction txn(env, extensions::graphdb::flags::txn::READ);
                       extensions::graphdb::Database db(txn, env.schema_[extensions::graphdb::schema::VERTEX_FEATURE], extensions::graphdb::flags::db::READ);
                       extensions::graphdb::mdb_view<uint64_t> mdbvB;
                       rc = db.get(key, mdbvB);
                       assert(MDB_SUCCESS == rc);

                       assertm(value == *(mdbvB.begin()), *(mdbvB.begin()));
                       assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                   }
               });

        mt.def("test_graphdb_find_slot",
               +[]{

                   using namespace extensions::graphdb;

                   std::string filename = "./test.db";
                   int rc;

                   Environment env(filename, schema::SCHEMA);

                   {
                       {
                           Transaction txnW(env, flags::txn::WRITE);
                           Database dbW(txnW, env.schema_[schema::L<schema::VERTEX_FEATURE>], flags::db::WRITE);

                           schema::list_key_t hint;
                           schema::list_key_t value;
                           mdb_view<schema::list_key_t> hint_k(&hint, 1);
                           mdb_view<schema::list_key_t> hint_v(&value, 1);

                           list::find_slot(dbW, hint_k, hint_v);
                           assertm(0 != hint_k.begin()->head(), hint_k.begin()->head(), hint_k.begin()->tail());
                           assertm(MDB_SUCCESS == (rc = txnW.commit()), rc);
                       }

                       {
                           Transaction txnR(env, flags::txn::READ);
                           Database dbR(txnR, env.schema_[schema::L<schema::VERTEX_FEATURE>], flags::db::READ);
                           mdb_view<schema::meta_key_t> meta_k(&schema::META_KEY_PAGE, 1);
                           mdb_view<schema::meta_page_t> meta_v;

                           assertm(MDB_SUCCESS == (rc = dbR.get(meta_k, meta_v)), rc);
                           assertm(1 == *(meta_v.begin()), *(meta_v.begin()));
                           assertm(MDB_SUCCESS == (rc = txnR.abort()), rc);
                       }

                       {
                           // fill in the first page
                           Transaction txnE(env, flags::txn::WRITE);
                           Database dbE(txnE, env.schema_[schema::L<schema::VERTEX_FEATURE>], flags::db::WRITE);

                           schema::list_key_t hint;
                           schema::list_key_t value;
                           for(typename schema::list_key_t::value_type h = 0; h < extensions::page_size * CHAR_BIT; ++h)
                           {
                               if(schema::RESERVED == h) continue;
                               hint.head() = h;
                               value.graph() = 0xba5eba11;
                               mdb_view<schema::list_key_t> hint_k(&hint, 1);
                               mdb_view<schema::list_key_t> hint_v(&value, 1);

                               assertm(MDB_SUCCESS == (rc = dbE.put(hint_k, hint_v, flags::put::DEFAULT)), rc);
                           }
                           assertm(MDB_SUCCESS == (rc = txnE.commit()), rc);
                       }

                       {
                           // trigger expansion
                           Transaction txn(env, flags::txn::WRITE);
                           Database db(txn, env.schema_[schema::L<schema::VERTEX_FEATURE>], flags::db::WRITE);

                           schema::list_key_t hint;
                           schema::list_key_t value;
                           mdb_view<schema::list_key_t> hint_k(&hint, 1);
                           mdb_view<schema::list_key_t> hint_v(&value, 1);

                           list::find_slot(db, hint_k, hint_v);
                           assertm(extensions::page_size * CHAR_BIT <= hint_k.begin()->head(), hint_k.begin()->head(), hint_k.begin()->tail());
                           assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                       }

                       {
                           Transaction txn(env, flags::txn::READ);
                           Database db(txn, env.schema_[schema::L<schema::VERTEX_FEATURE>], flags::db::READ);
                           mdb_view<schema::meta_key_t> meta_k(&schema::META_KEY_PAGE, 1);
                           mdb_view<schema::meta_page_t> meta_v;

                           assertm(MDB_SUCCESS == (rc = db.get(meta_k, meta_v)), rc);
                           assertm(2 == *(meta_v.begin()), *(meta_v.begin()));
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

                   Environment env(filename, schema::SCHEMA);

                   using elem_t = uint16_t;
                   using buff_t = std::array<elem_t, extensions::page_size>;  // == 2 pages
                   size_t chunk = extensions::page_size / sizeof(elem_t);

                   {
                       buff_t buffer{0};
                       buffer[0] = 0xba5e;
                       buffer[chunk-1] = 0xba11;
                       buffer[chunk] = 0xc001;
                       buffer[2*chunk-1] = 0xbee5;

                       Transaction txn(env, flags::txn::WRITE);
                       Database db(txn, env.schema_[schema::H<schema::VERTEX_FEATURE>], flags::db::WRITE);

                       schema::list_key_t head{1,0};
                       mdb_view<schema::list_key_t> head_k(&head, 1);

                       mdb_view<elem_t> head_v(buffer.data(), buffer.size());

                       list::write(db, head_k, head_v);

                       // verify the DB

                       schema::list_key_t iter;
                       mdb_view<schema::list_key_t> iter_k(&iter, 1);
                       mdb_view<elem_t> iter_v;

                       iter = schema::list_key_t{1,0};
                       assertm(MDB_SUCCESS == (rc = db.get(iter_k, iter_v)), rc);

                       assertm(0xba5e == *(iter_v.begin()), *(iter_v.begin()));
                       assertm(0      == *(iter_v.begin() + 1), *(iter_v.begin() + 1));
                       assertm(0      == *(iter_v.begin() + chunk - 2), *(iter_v.begin() + chunk - 2));
                       assertm(0xba11 == *(iter_v.begin() + chunk - 1), *(iter_v.begin() + chunk - 1));

                       iter = schema::list_key_t{1,1};
                       assertm(MDB_SUCCESS == (rc = db.get(iter_k, iter_v)), rc);

                       assertm(0xc001 == *(iter_v.begin()),     *(iter_v.begin()));
                       assertm(0      == *(iter_v.begin() + 1), *(iter_v.begin() + 1));
                       assertm(0      == *(iter_v.begin() + chunk - 2), *(iter_v.begin() + chunk - 2));
                       assertm(0xbee5 == *(iter_v.begin() + chunk - 1), *(iter_v.begin() + chunk - 1));

                       iter = schema::list_key_t{1,2};
                       assertm(MDB_NOTFOUND == (rc = db.get(iter_k, iter_v)), rc);

                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       Transaction txn(env, flags::txn::NESTED);
                       Database db(txn, env.schema_[schema::H<schema::VERTEX_FEATURE>], flags::db::READ);

                       buff_t buffer{0};
                       schema::list_key_t head{1,0};
                       mdb_view<schema::list_key_t> head_k(&head, 1);
                       mdb_view<elem_t> head_v(buffer.data(), buffer.size());

                       list::read(db, head_k, head_v);

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

                   Environment env(filename, schema::SCHEMA);

                   using elem_t = uint16_t;
                   using buff_t = std::array<elem_t, extensions::page_size>;  // == 2 pages
                   size_t chunk = extensions::page_size / sizeof(elem_t);

                   {
                       buff_t buffer{0};
                       buffer[0] = 0xba5e;
                       buffer[chunk-1] = 0xba11;
                       buffer[chunk] = 0xc001;
                       buffer[2*chunk-1] = 0xbee5;

                       Transaction txn(env, flags::txn::WRITE);
                       Database db(txn, env.schema_[schema::H<schema::VERTEX_FEATURE>], flags::db::WRITE);

                       schema::list_key_t head{1,0};
                       mdb_view<schema::list_key_t> head_k(&head, 1);

                       mdb_view<elem_t> head_v(buffer.data(), buffer.size());

                       list::write(db, head_k, head_v);

                       // verify the DB

                       schema::list_key_t iter;
                       mdb_view<schema::list_key_t> iter_k(&iter, 1);
                       mdb_view<elem_t> iter_v;

                       iter = schema::list_key_t{1,0};
                       assertm(MDB_SUCCESS == (rc = db.get(iter_k, iter_v)), rc);

                       iter = schema::list_key_t{1,1};
                       assertm(MDB_SUCCESS == (rc = db.get(iter_k, iter_v)), rc);

                       iter = schema::list_key_t{1,2};
                       assertm(MDB_NOTFOUND == (rc = db.get(iter_k, iter_v)), rc);

                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       schema::list_key_t head{1,0};
                       mdb_view<schema::list_key_t> head_k(&head, 1);

                       Transaction txn(env, flags::txn::WRITE);
                       Database db(txn, env.schema_[schema::H<schema::VERTEX_FEATURE>], flags::db::WRITE);

                       list::clear(db, head_k);

                       // verify the DB

                       schema::list_key_t iter;
                       mdb_view<schema::list_key_t> iter_k(&iter, 1);
                       mdb_view<elem_t> iter_v;

                       iter = schema::list_key_t{1,0};
                       assertm(MDB_NOTFOUND == (rc = db.get(iter_k, iter_v)), rc);

                       iter = schema::list_key_t{1,1};
                       assertm(MDB_NOTFOUND == (rc = db.get(iter_k, iter_v)), rc);

                       iter = schema::list_key_t{1,2};
                       assertm(MDB_NOTFOUND == (rc = db.get(iter_k, iter_v)), rc);

                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

               });

        mt.def("test_graphdb_hash_write",
               +[]{
                   using namespace extensions::graphdb;

                   std::string filename = "./test.db";
                   int rc;

                   Environment env(filename, schema::SCHEMA);

                   {
                       Transaction txn(env, flags::txn::WRITE);
                       Database db(txn, env.schema_[schema::H<schema::VERTEX_FEATURE>], flags::db::WRITE);

                       schema::list_key_t hash{1,0};
                       mdb_view<schema::list_key_t> hash_k(&hash, 1);

                       schema::list_key_t value;
                       mdb_view<schema::list_key_t> hash_v(&value, 1);

                       value = {0xba5e, 0xba11};
                       hash::write(db, hash_k, hash_v);

                       value = {0xdead, 0xbeef};
                       hash::write(db, hash_k, hash_v);

                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       Transaction txn(env, flags::txn::READ);
                       Database db(txn, env.schema_[schema::H<schema::VERTEX_FEATURE>], flags::db::READ);

                       schema::list_key_t hash;
                       mdb_view<schema::list_key_t> hash_k(&hash, 1);
                       mdb_view<schema::list_key_t> hash_v;

                       hash = {1,0};

                       assertm(MDB_SUCCESS == (rc = db.get(hash_k, hash_v)), rc);
                       assertm(0xba5e == hash_v.begin()->head(), hash_v.begin()->head());
                       assertm(0xba11 == hash_v.begin()->tail(), hash_v.begin()->tail());

                       hash = {1,1};

                       assertm(MDB_SUCCESS == (rc = db.get(hash_k, hash_v)), rc);
                       assertm(0xdead == hash_v.begin()->head(), hash_v.begin()->head());
                       assertm(0xbeef == hash_v.begin()->tail(), hash_v.begin()->tail());

                       assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                   }
               });

        mt.def("test_graphdb_hash_remove",
               +[]{
                   using namespace extensions::graphdb;

                   std::string filename = "./test.db";
                   int rc;

                   Environment env(filename, schema::SCHEMA);

                   {
                       Transaction txn(env, flags::txn::NESTED);
                       Database db(txn, env.schema_[schema::H<schema::VERTEX_FEATURE>], flags::db::WRITE);

                       schema::list_key_t hash{1,0};
                       mdb_view<schema::list_key_t> hash_k(&hash, 1);

                       schema::list_key_t value;
                       mdb_view<schema::list_key_t> hash_v(&value, 1);

                       value = {0xba5e, 0xba11};
                       hash::write(db, hash_k, hash_v);

                       value = {0xdead, 0xbeef};
                       hash::write(db, hash_k, hash_v);

                       value = {0xc001, 0xbee5};
                       hash::write(db, hash_k, hash_v);

                       assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                   }

                   {
                       Transaction txn(env, flags::txn::READ);
                       Database db(txn, env.schema_[schema::H<schema::VERTEX_FEATURE>], flags::db::READ);

                       schema::list_key_t hash;
                       mdb_view<schema::list_key_t> hash_k(&hash, 1);
                       mdb_view<schema::list_key_t> hash_v;

                       hash = {1,0};
                       assertm(MDB_SUCCESS == (rc = db.get(hash_k, hash_v)), rc);

                       hash = {1,1};
                       assertm(MDB_SUCCESS == (rc = db.get(hash_k, hash_v)), rc);

                       hash = {1,2};
                       assertm(MDB_SUCCESS == (rc = db.get(hash_k, hash_v)), rc);

                       hash = {1,3};
                       assertm(MDB_NOTFOUND == (rc = db.get(hash_k, hash_v)), rc);

                       assertm(MDB_SUCCESS == (rc = txn.abort()), rc);
                   }

                   {
                       Transaction txn(env, flags::txn::NESTED);
                       Database db(txn, env.schema_[schema::H<schema::VERTEX_FEATURE>], flags::db::WRITE);

                       {
                           schema::list_key_t hash;
                           mdb_view<schema::list_key_t> hash_k(&hash, 1);

                           hash = {1,0};
                           list::remove(db, hash_k);
                       }

                       {
                           schema::list_key_t iter;
                           mdb_view<schema::list_key_t> iter_k(&iter, 1);
                           mdb_view<schema::list_key_t> iter_v;

                           iter = {1,1};
                           assertm(MDB_SUCCESS == (rc = db.get(iter_k, iter_v)), rc);

                           iter = {1,2};
                           assertm(MDB_NOTFOUND == (rc = db.get(iter_k, iter_v)), rc);

                           iter = {1,3};
                           assertm(MDB_NOTFOUND == (rc = db.get(iter_k, iter_v)), rc);

                           assertm(MDB_SUCCESS == (rc = txn.commit()), rc);
                       }
                   }
               });
    }
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    PYBIND11_MODULE_IMPL(m);
}
