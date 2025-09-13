#include "../system.h"

extern "C"
{
#include "lmdb.h"
}

#include "db.h"
#include "schema.h"
#include "envpool.h"

typename extensions::graphdb::EnvironmentPool::map_t extensions::graphdb::EnvironmentPool::pool_ = {};
