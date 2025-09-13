#include "../../system.h"
#include "parallel/module.h"
#include "linalg/module.h"

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    extensions::theory::parallel::PYBIND11_MODULE_PARALLEL(m);
    extensions::theory::linalg::PYBIND11_MODULE_MATRIX(m);
}
