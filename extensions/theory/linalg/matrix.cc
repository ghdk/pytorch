#include "../../system.h"
#include "gauss_jordan.h"
#include "matrix.h"

void PYBIND11_MODULE_IMPL(py::module_ m)
{
    auto mm = m.def_submodule("matrix", "");
    mm.def("random", &extensions::theory::linalg::matrix::random);
    mm.def("copy", &extensions::theory::linalg::matrix::copy);
    mm.def("reorder", static_cast<torch::Tensor(*)(torch::Tensor,torch::Tensor)>(&extensions::theory::linalg::matrix::reorder));
    mm.def("augment", &extensions::theory::linalg::matrix::augment);
    mm.def("identity", &extensions::theory::linalg::matrix::identity);
    mm.def("sum", &extensions::theory::linalg::matrix::sum);
    mm.def("product", &extensions::theory::linalg::matrix::product);
    mm.def("transpose", &extensions::theory::linalg::matrix::transpose);
    mm.def("reverse", &extensions::theory::linalg::matrix::reverse);
    mm.def("basis", &extensions::theory::linalg::matrix::basis);
    mm.def("rank", &extensions::theory::linalg::matrix::rank);
    mm.def("ref", +[](torch::Tensor A) -> torch::Tensor
           {
                torch::Tensor R = extensions::theory::linalg::matrix::copy(A);
                extensions::theory::linalg::gauss_jordan::downward(R);
                return R;
           });
    mm.def("rref", +[](torch::Tensor A) -> torch::Tensor
           {
                torch::Tensor R = extensions::theory::linalg::matrix::copy(A);
                extensions::theory::linalg::gauss_jordan::downward(R);
                extensions::theory::linalg::gauss_jordan::upward(R);
                return R;
           });
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    PYBIND11_MODULE_IMPL(m);
}
