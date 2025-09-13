#ifndef EXTENSIONS_THEORY_LINALG_GAUSS_JORDAN_H
#define EXTENSIONS_THEORY_LINALG_GAUSS_JORDAN_H

namespace extensions { namespace theory { namespace linalg { namespace gauss_jordan
{

/*
 * Gauss-Jordan elimination consists of two phases. The first phase brings
 * the system in row echelon form (REF), in the second phase in reduced row
 * echelon form (RREF).
 *
 * We implement these two phases separately. The downward phase generates the
 * REF by zeroing out the bottom left triangle of the matrix. The upward phase
 * generates the RREF, by a) scaling the pivot entries to 1 and b) zeroing out
 * the top right triangle of the matrix.
 */

void downward(torch::Tensor A)
{
    int64_t m = A.sizes()[0];
    int64_t n = A.sizes()[1];

    auto kernel = [m,n](auto A)
    {
        int64_t pivot_r = 0;
        int64_t pivot_c = 0;

        while(pivot_r < m and pivot_c < n)
        {
            if(!A[pivot_r][pivot_c])
                pivot_c++;
            else
            {
                for(int64_t i = pivot_r+1; i < m; i++)
                {
                    // Zero out the rows below the pivot point.
                    float f = A[pivot_r][pivot_c] ? A[i][pivot_c] / A[pivot_r][pivot_c] : 0;

                    for(int64_t j = pivot_c; j < n; j++)
                        A[i][j] = A[i][j] - A[pivot_r][j] * f;
                }
                pivot_r++;
            }
        }
    };

    if(A.dtype() == torch::kInt64)
    {
        auto acc = A.accessor<int64_t, 2UL>();
        kernel(acc);
    }
    if(A.dtype() == torch::kFloat)
    {
        auto acc = A.accessor<float, 2UL>();
        kernel(acc);
    }
}

void upward(torch::Tensor A)
{
    auto m = A.sizes()[0];
    auto n = A.sizes()[1];

    auto kernel = [m,n](auto A)
    {
        int64_t pivot_r = m-1;
        int64_t pivot_c = n-1;

        while(pivot_r >= 0)
        {
            // Find the left most column with a non-zero value, this is the pivot.
            for(int64_t j = pivot_c; j >= 0; j--)
                pivot_c = A[pivot_r][j] ? j : pivot_c;

            for(int64_t i = pivot_r; i >= 0; i--)
            {
                if(i == pivot_r)
                {
                    // Scale down the pivot row so that the pivot entry is one.
                    float f = A[i][pivot_c];
                    for(int64_t j = pivot_c; j < n; j++)
                        A[i][j] = A[i][j] / f;
                }
                else
                {
                    // Zero out the rows above the pivot point.
                    float f = A[pivot_r][pivot_c] ? A[i][pivot_c] / A[pivot_r][pivot_c] : 0;

                    for(int64_t j = pivot_c; j < n; j++)
                        A[i][j] = A[i][j] - A[pivot_r][j] * f;
                }
            }
            pivot_r--;
        }
    };

    if(A.dtype() == torch::kInt64)
    {
        auto acc = A.accessor<int64_t, 2UL>();
        kernel(acc);
    }
    if(A.dtype() == torch::kFloat)
    {
        auto acc = A.accessor<float, 2UL>();
        kernel(acc);
    }
}

}}}}  // namespace extensions::theory::linalg::gauss_jordan

#endif  // EXTENSIONS_THEORY_LINALG_GAUSS_JORDAN_H
