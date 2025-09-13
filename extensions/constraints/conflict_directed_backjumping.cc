#include "../system.h"
#include "accessor.h"
#include "conflict_directed_backjumping.h"

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    using namespace extensions::constraints;
    auto c = py::class_<ConflictDirectedBackjumping, extensions::ptr_t<ConflictDirectedBackjumping>>(m, "ConflictDirectedBackjumping");
    ConflictDirectedBackjumping::def(c);
 }
