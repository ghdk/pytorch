#ifndef EXTENSIONS_GRAPHDB_ENVPOOL_H
#define EXTENSIONS_GRAPHDB_ENVPOOL_H

namespace extensions { namespace graphdb
{

class EnvironmentPool
{
public:
    static Environment& environment(std::string const& filename)
    {
        pool_.try_emplace(filename, filename, extensions::graphdb::schema::SCHEMA, extensions::graphdb::flags::env::DEFAULT);
        return pool_.at(filename);
    }

private:
    using map_t = std::unordered_map<std::string, Environment>;
private:
    static map_t pool_;
};

typename EnvironmentPool::map_t EnvironmentPool::pool_ = {};

}};  // extensions::graphdb
#endif  // EXTENSIONS_GRAPHDB_ENVPOOL_H
