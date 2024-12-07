#ifndef EXTENSIONS_RSSE_SCHEMA_H
#define EXTENSIONS_RSSE_SCHEMA_H

namespace extensions { namespace rsse { namespace schema
{

const char* DB_FILE = "llvmast.db";

// A single graph holds all the information for a given code repository.
// This offers the highest expressivity.
constexpr graphdb::schema::graph_vtx_set_key_t GRAPH = {0};

namespace feature
{
constexpr graphdb::schema::vertex_feature_key_t::value_type UNDEF     = {0};
constexpr graphdb::schema::vertex_feature_key_t::value_type TYPE      = {1};
constexpr graphdb::schema::vertex_feature_key_t::value_type NAME      = {2};
}  // namespace feature

namespace type
{
// The values of the TYPE feature.
constexpr graphdb::schema::vertex_feature_key_t::value_type UNDEF         = {0};
constexpr graphdb::schema::vertex_feature_key_t::value_type FILE          = {1};
constexpr graphdb::schema::vertex_feature_key_t::value_type RECORD_DECL   = {2};
constexpr graphdb::schema::vertex_feature_key_t::value_type FUNCTION_DECL = {3};
}  // namespace type

}}}  // namespace extensions::rsse::schema

#endif  // EXTENSIONS_RSSE_SCHEMA_H
