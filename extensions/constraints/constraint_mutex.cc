#include "../system.h"
#include "accessor.h"
#include "constraint_mutex.h"

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    using namespace extensions::constraints;
    auto c = py::class_<ConstraintMutex, extensions::ptr_t<ConstraintMutex>>(m, "ConstraintMutex");
    ConstraintMutex::def(c);
 }
