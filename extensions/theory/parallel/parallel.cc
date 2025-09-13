#include "../../system.h"
#include "memmodel.h"
#include "parallel.h"
#include "koggestone.h"
#include "sort.h"
#include "module.h"

void extensions::theory::parallel::PYBIND11_MODULE_PARALLEL(py::module_ m)
{
    auto mm = m.def_submodule("parallel", "");

    mm.def("copy", static_cast<void(*)(torch::Tensor, torch::Tensor, size_t, size_t)>(&extensions::theory::parallel::copy));

    /**
     * When Python code calls C++, and vice versa, the GIL is held.
     * Hence if we call launch from Python, the call will acquire
     * the GIL, preventing the threads from calling the Python kernel,
     * effectively deadlocking.
     *
     * See https://pybind11.readthedocs.io/en/stable/advanced/misc.html#global-interpreter-lock-gil
     */
    mm.def("fanout", &extensions::theory::parallel::fanout, py::call_guard<py::gil_scoped_release>());
    mm.def("koggestone", &extensions::theory::parallel::koggestone::launch);
    mm.def("bitonic", &extensions::theory::parallel::sort::bitonic);
}
