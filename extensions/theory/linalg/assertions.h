#ifndef EXTENSIONS_THEORY_LINALG_ASSERTIONS_H
#define EXTENSIONS_THEORY_LINALG_ASSERTIONS_H

namespace extensions { namespace theory { namespace linalg { namespace assertions
{

void assert_matrices_are_of_equal_dimensions(torch::Tensor A, torch::Tensor B)
{
    assertm(A.sizes() == B.sizes(), "Expecting same dimensions:", A.sizes(), "==", B.sizes());
}

void assert_matrices_are_of_the_same_type(torch::Tensor A, torch::Tensor B)
{
    assertm(A.dtype() == B.dtype(), "Expecting same type:", A.dtype(), "==", B.dtype());
}

void assert_matrix_is_square(torch::Tensor M)
{
    assertm(M.dim() == 2, M.dim());
    assertm(M.sizes()[0] == M.sizes()[1], M.sizes());
}

}}}}  // namespace extensions::theory::linalg::assertions

#endif  // EXTENSIONS_THEORY_LINALG_ASSERTIONS_H
