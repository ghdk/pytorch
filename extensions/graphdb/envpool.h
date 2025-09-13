#ifndef EXTENSIONS_GRAPHDB_ENVPOOL_H
#define EXTENSIONS_GRAPHDB_ENVPOOL_H

namespace extensions { namespace graphdb
{

class EnvironmentPool
{
public:
    static Environment& environment(std::string const& filename)
    {
        std::string db = std::filesystem::current_path().string() + "/" + filename;
        pool_.try_emplace(db, db, extensions::graphdb::schema::SCHEMA, extensions::graphdb::flags::env::DEFAULT);
        return pool_.at(db);
    }

private:
    using map_t = std::unordered_map<std::string, Environment>;
private:
    VISIBLE static map_t pool_;
};

}};  // extensions::graphdb
#endif  // EXTENSIONS_GRAPHDB_ENVPOOL_H
