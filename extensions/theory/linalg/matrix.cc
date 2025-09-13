#include "../../system.h"
#include "assertions.h"
#include "module.h"
#include "gauss_jordan.h"
#include "matrix.h"

void extensions::theory::linalg::PYBIND11_MODULE_MATRIX(py::module_ m)
{
    auto mm = m.def_submodule("matrix", "");
    mm.def("basis", &extensions::theory::linalg::matrix::basis);
    mm.def("augment", &extensions::theory::linalg::matrix::augment);
    mm.def("copy", &extensions::theory::linalg::matrix::copy);
    mm.def("determinant", &extensions::theory::linalg::matrix::determinant);
    mm.def("identity", &extensions::theory::linalg::matrix::identity);
    mm.def("inverse", &extensions::theory::linalg::matrix::inverse);
    mm.def("is_symmetric", &extensions::theory::linalg::matrix::is_symmetric);
    mm.def("product", &extensions::theory::linalg::matrix::product);
    mm.def("random", &extensions::theory::linalg::matrix::random);
    mm.def("rank", &extensions::theory::linalg::matrix::rank);
    mm.def("ref", +[](torch::Tensor A) -> torch::Tensor
           {
                torch::Tensor R = extensions::theory::linalg::matrix::copy(A);
                extensions::theory::linalg::gauss_jordan::downward(R);
                return R;
           });
    mm.def("reorder", static_cast<torch::Tensor(*)(torch::Tensor,torch::Tensor)>(&extensions::theory::linalg::matrix::reorder));
    mm.def("rref", +[](torch::Tensor A) -> torch::Tensor
           {
                torch::Tensor R = extensions::theory::linalg::matrix::copy(A);
                extensions::theory::linalg::gauss_jordan::downward(R);
                extensions::theory::linalg::gauss_jordan::upward(R);
                return R;
           });
    mm.def("sum", &extensions::theory::linalg::matrix::sum);
    mm.def("trace", &extensions::theory::linalg::matrix::trace);
    mm.def("transition", &extensions::theory::linalg::matrix::transition);
    mm.def("transpose", &extensions::theory::linalg::matrix::transpose);
}
