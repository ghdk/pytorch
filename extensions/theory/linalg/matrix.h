#ifndef EXTENSIONS_THEORY_LINALG_MATRIX_H
#define EXTENSIONS_THEORY_LINALG_MATRIX_H

namespace extensions { namespace theory { namespace linalg { namespace matrix
{

torch::Tensor random(int64_t m, int64_t n)
{
    torch::Tensor R = torch::randint(0, m*n, {m,n});
    return R;
}

torch::Tensor copy(torch::Tensor M)
{
    torch::Tensor R = M.detach().clone();
    return R;
}

torch::Tensor reorder(torch::Tensor M, torch::Tensor order)
{
    torch::Tensor R = M.index_select(0, order);
    return R;
}

torch::Tensor augment(torch::Tensor A, torch::Tensor B)
{
    assertm(A.sizes()[0] == B.sizes()[0], "Expecting same number of rows:", A.sizes(), "==", B.sizes());
    assertm(A.dtype() == B.dtype(), "Expecting same type:", A.dtype(), "==", B.dtype());

    torch::Tensor R = torch::zeros({A.sizes()[0], A.sizes()[1] + B.sizes()[1]},
                                     A.options());

    R.slice(0 /* rows */,
              0 /* start row */,
              A.sizes()[0] /* end row */)
       .slice(1 /* columns */,
              0 /* start column */,
              A.sizes()[1] /* end column */) = A.slice(0, 0, A.sizes()[0])
                                                .slice(1, 0, A.sizes()[1]);

    R.slice(0 /* rows */,
              0 /* start row */,
              B.sizes()[0] /* end row */)
       .slice(1 /* columns */,
              A.sizes()[1] /* start column */,
              A.sizes()[1] + B.sizes()[1] /* end column */) = B.slice(0, 0, B.sizes()[0])
                                                               .slice(1, 0, B.sizes()[1]);

    return R;
}

torch::Tensor identity(int64_t m)  // Has to be square.
{
    auto opt = torch::TensorOptions().dtype(torch::kInt64)
                                      .requires_grad(false);
    torch::Tensor R = torch::zeros({m, m}, opt);
    auto acc = R.accessor<int64_t, 2UL>();

    for(int64_t i = 0; i < m; i++)
        acc[i][i] = 1;

    return R;
}

torch::Tensor sum(torch::Tensor A, torch::Tensor B)
{
    assertm(A.sizes() == B.sizes(), "Expecting same dimensions:", A.sizes(), "==", B.sizes());
    assertm(A.dtype() == B.dtype(), "Expecting same type:", A.dtype(), "==", B.dtype());

    int64_t m = A.sizes()[0];
    int64_t n = A.sizes()[1];

    torch::Tensor R = torch::zeros({m, n}, A.options());

    auto kernel = [n,m](auto R, auto A, auto B)
    {
        for(int64_t row_i = 0; row_i < m; row_i++)
            for(int64_t column_j = 0; column_j < n; column_j++)
                R[row_i][column_j] = A[row_i][column_j] + B[row_i][column_j];
    };

    if(R.dtype() == torch::kInt64)
    {
        auto acc = R.accessor<int64_t, 2UL>();
        auto Aacc = A.accessor<int64_t, 2UL>();
        auto Bacc = B.accessor<int64_t, 2UL>();
        kernel(acc, Aacc, Bacc);
    }
    if(R.dtype() == torch::kFloat)
    {
        auto acc = R.accessor<float, 2UL>();
        auto Aacc = A.accessor<float, 2UL>();
        auto Bacc = B.accessor<float, 2UL>();
        kernel(acc, Aacc, Bacc);
    }

    return R;
}

torch::Tensor product(torch::Tensor A, torch::Tensor B)
{
    assertm(A.dtype() == B.dtype(), "Expecting same type:", A.dtype(), "==", B.dtype());

    // We cannot add matrices of different dimensions, but we can multiply them
    // as long as the number of columns of A equals the number of rows of B.

    int64_t m = B.sizes()[0];
    int64_t n = A.sizes()[1];

    assertm(m == n, "Expecting:", m, "==", n);

    // The result will have the same number of rows as A and columns as B.
    int64_t m_ = A.sizes()[0];
    int64_t n_ = B.sizes()[1];

    torch::Tensor R = torch::zeros({m_,n_}, A.options());

    auto kernel = [m,m_,n_](auto R, auto A, auto B)
    {
        for(int64_t row_i = 0; row_i < m_; row_i++)
            for(int64_t column_j = 0; column_j < n_; column_j++)
            {

                auto row_a = row_i;
                auto column_b = column_j;

                for(int64_t i = 0; i < m; i++)
                    R[row_i][column_j] += A[row_a][i] * B[i][column_b];
            }
    };

    if(R.dtype() == torch::kInt64)
    {
        auto acc = R.accessor<int64_t, 2UL>();
        auto Aacc = A.accessor<int64_t, 2UL>();
        auto Bacc = B.accessor<int64_t, 2UL>();
        kernel(acc, Aacc, Bacc);
    }
    if(R.dtype() == torch::kFloat)
    {
        auto acc = R.accessor<float, 2UL>();
        auto Aacc = A.accessor<float, 2UL>();
        auto Bacc = B.accessor<float, 2UL>();
        kernel(acc, Aacc, Bacc);
    }

    return R;
}

torch::Tensor transpose(torch::Tensor A)
{
    int64_t m = A.sizes()[0];
    int64_t n = A.sizes()[1];

    torch::Tensor R = torch::zeros({n,m}, A.options());

    auto kernel = [m,n](auto R, auto A)
    {
        for(int64_t row_i = 0; row_i < m; row_i++)
            for(int64_t column_j = 0; column_j < n; column_j++)
                R[column_j][row_i] = A[row_i][column_j];
    };

    if(R.dtype() == torch::kInt64)
    {
        auto acc = R.accessor<int64_t, 2UL>();
        auto Aacc = A.accessor<int64_t, 2UL>();
        kernel(acc, Aacc);
    }
    if(R.dtype() == torch::kFloat)
    {
        auto acc = R.accessor<float, 2UL>();
        auto Aacc = A.accessor<float, 2UL>();
        kernel(acc, Aacc);
    }

    return R;
}

bool is_symmetric(torch::Tensor A)
{
    torch::Tensor T = transpose(A);
    return A.equal(T);
}

torch::Tensor inverse(torch::Tensor A)
{
    // A * A^(-1) = I
    int64_t m = A.sizes()[0];
    torch::Tensor I = identity(m);
    I = I.to(A.dtype());

    torch::Tensor R = augment(A,I);

    gauss_jordan::downward(R);
    gauss_jordan::upward(R);

    // Extract the identity matrix that should now hold the inverse matrix.
    R = R.slice(0, R.sizes()[0] - m, R.sizes()[0])
         .slice(1, R.sizes()[1] - m, R.sizes()[1]);

    return R;
}

torch::Tensor basis(torch::Tensor A, torch::Tensor REF)
{
    /*
     * A is a matrix whose columns are vectors. The vectors in A define a set
     * which spans a vector space. REF is a matrix holding the row echelon
     * form of A.
     *
     * Returns a new matrix, with the same size as A, containing only the
     * columns of A that form the basis of the vector space.
     */

    int64_t m = A.sizes()[0];
    int64_t n = A.sizes()[1];

    torch::Tensor R = copy(REF);

    auto kernel = [m,n](auto R, auto A)
    {
        int64_t pivot_r = 0;
        int64_t pivot_c = 0;

        while(pivot_r < m and pivot_c < n)
        {
            if(0 == R[pivot_r][pivot_c])
                pivot_c += 1;
            else
            {
                R[m-1][pivot_c] = 1;
                pivot_r += 1;
            }
        }

        pivot_r = m-1;
        pivot_c = n-1;

        while(pivot_c >= 0)
        {
            for(int64_t i = 0; i <= pivot_r; i++)
                R[i][pivot_c] = R[pivot_r][pivot_c] ? A[i][pivot_c] : 0;
            pivot_c -= 1;
        }
    };

    if(R.dtype() == torch::kInt64)
    {
        auto acc = R.accessor<int64_t, 2UL>();
        auto Aacc = A.accessor<int64_t, 2UL>();
        kernel(acc, Aacc);
    }
    if(R.dtype() == torch::kFloat)
    {
        auto acc = R.accessor<float, 2UL>();
        auto Aacc = A.accessor<float, 2UL>();
        kernel(acc, Aacc);
    }

    return R;
}

torch::Tensor rank(torch::Tensor REF)
{
    // REF is a matrix holding the row echelon form of a matrix.

    int64_t m = REF.sizes()[0];
    int64_t n = REF.sizes()[1];

    torch::Tensor ret = torch::zeros({1}, REF.options());

    auto kernel = [m,n,&ret](auto REF)
    {
        int64_t pivot_r = 0;
        int64_t pivot_c = 0;

        using held_t = extensions::accessor_held_t<decltype(REF)>;
        held_t sum = (held_t)0;

        while(pivot_r < m and pivot_c < n)
        {
            if(0 == REF[pivot_r][pivot_c])
                pivot_c += 1;
            else
            {
                sum += 1;
                pivot_r += 1;
            }
        }

        ret[0] = sum;
    };

    if(REF.dtype() == torch::kInt64)
    {
        auto acc = REF.accessor<int64_t, 2UL>();
        kernel(acc);
    }
    if(REF.dtype() == torch::kFloat)
    {
        auto acc = REF.accessor<float, 2UL>();
        kernel(acc);
    }

    return ret;
}

torch::Tensor determinant(torch::Tensor REF)
{
    // REF is a matrix holding the row echelon form of a matrix.

    int64_t m = REF.sizes()[0];
    int64_t n = REF.sizes()[1];

    torch::Tensor ret = torch::zeros({1}, REF.options());

    auto kernel = [m,n,&ret](auto REF)
    {
        int64_t pivot_r = 0;
        int64_t pivot_c = 0;

        using held_t = extensions::accessor_held_t<decltype(REF)>;
        held_t determinant = (held_t)1;

        while(pivot_r < m and pivot_c < n)
        {
            determinant *= REF[pivot_r][pivot_c];
            pivot_c += 1;
            pivot_r += 1;
        }

        ret[0] = determinant;
    };

    if(REF.dtype() == torch::kInt64)
    {
        auto acc = REF.accessor<int64_t, 2UL>();
        kernel(acc);
    }
    if(REF.dtype() == torch::kFloat)
    {
        auto acc = REF.accessor<float, 2UL>();
        kernel(acc);
    }

    return ret;
}

torch::Tensor trace(torch::Tensor A)
{
    int64_t m = A.sizes()[0];
    int64_t n = A.sizes()[1];

    assertm(m == n, "Expecting:", m, "==", n);

    torch::Tensor ret = torch::zeros({1}, A.options());

    auto kernel = [m,&ret](auto A)
    {
        using held_t = extensions::accessor_held_t<decltype(A)>;
        held_t sum = (held_t)0;

        for(size_t i = 0; i < m; i++)
        {
            sum += A[i][i];
        }

        ret[0] = sum;
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
    return ret;
}

torch::Tensor transition(torch::Tensor FROM, torch::Tensor TO)
{
    // Calculate the transition matrix, Elementary Linear Algebra, eq. 14, p. 260.
    assertions::assert_matrices_are_of_equal_dimensions(FROM, TO);
    assertions::assert_matrices_are_of_the_same_type(FROM, TO);
    assertions::assert_matrix_is_square(FROM);

    torch::Tensor R = augment(TO,FROM);

    gauss_jordan::downward(R);
    gauss_jordan::upward(R);

    // [TO|FROM] becomes [I|transition matrix]
    R = R.slice(0, R.sizes()[0] - TO.sizes()[0], R.sizes()[0])
         .slice(1, R.sizes()[1] - TO.sizes()[1], R.sizes()[1]);

    return R;
}

}}}}  // namespace extensions::theory::linalg::matrix

#endif  // EXTENSIONS_THEORY_LINALG_MATRIX_H
