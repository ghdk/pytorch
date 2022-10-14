#include "../system.h"
#include "accessor.h"
#include "forward_checking.h"

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    using namespace extensions::constraints;
    auto c = py::class_<ForwardChecking, extensions::ptr_t<ForwardChecking>>(m, "ForwardChecking");
    ForwardChecking::def(c);
 }
