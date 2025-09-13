#ifndef EXTENSIONS_THEORY_PARALLEL_KOGGESTONE_H
#define EXTENSIONS_THEORY_PARALLEL_KOGGESTONE_H

namespace extensions { namespace theory { namespace parallel { namespace koggestone
{

/*
 * Expects that the input tensor is the same size as the output tensor. The
 * right most element of the output tensor holds the result.
 */
void launch(torch::Tensor ib, torch::Tensor ob)
{
    torch::Tensor sb = extensions::theory::parallel::memmodel::shared_memory(ib.options());

    auto delegate = [](auto ia, auto oa, auto sa)
    {

        // Kogge-Stone
        // Programming Massively Parallel Processors, ch. 11.2.
        //
        // The operator that is applied on the elements of the data set
        // needs to be associative. The right most tile of each region
        // (size of the thread pool) will hold the sum for the region.

        auto koggestone = [&](int64_t begin, int64_t end)
        {
            using held_t = extensions::accessor_held_t<decltype(sa)>;

            int64_t thread_i = begin;
            for(int64_t offset = 0; offset < sa.size(0); offset += memmodel::tile_sz)
            {
                for(int64_t stride = 1; stride < sa.size(0); stride = stride * 2)
                {
                    extensions::theory::parallel::memmodel::Barrier::wait(begin);
                    held_t temp = 0;
                    if(thread_i >= stride)
                        temp = sa[offset + thread_i] + sa[offset + thread_i - stride];
                    extensions::theory::parallel::memmodel::Barrier::wait(begin);
                    if(thread_i >= stride)
                        sa[offset + thread_i] = temp;
                }
            }
        };

        for(int64_t offset = 0; offset < ia.size(0); offset += sa.size(0))
        {
            extensions::theory::parallel::copy(ia, sa, offset, 0);
            extensions::theory::parallel::fanout(koggestone);
            extensions::theory::parallel::copy(sa, oa, 0, offset);
        }

        // Thread 0 sums the right most value of each tile, to the
        // right most value of the entire buffer. Instead we could
        // use a sum array that holds the sum of each tile, and then
        // do a parallel scan on the sum array.
        auto kernel = [&](int64_t begin, int64_t end)
        {
            int64_t thread_i = begin;
            if(thread_i) return;
            for(int64_t i = memmodel::tile_sz - 1; i < oa.size(0) - 1; i += memmodel::tile_sz)
                oa[oa.size(0) - 1] += oa[i];

        };
        extensions::theory::parallel::fanout(kernel);
    };

    if(sb.dtype() == torch::kInt64)
    {
        auto ia = ib.accessor<int64_t, 1UL>();
        auto oa = ob.accessor<int64_t, 1UL>();
        auto sa = sb.accessor<int64_t, 1UL>();
        delegate(ia, oa, sa);
    }
    if(sb.dtype() == torch::kFloat)
    {
        auto ia = ib.accessor<float, 1UL>();
        auto oa = ob.accessor<float, 1UL>();
        auto sa = sb.accessor<float, 1UL>();
        delegate(ia, oa, sa);
    }
}

}}}}  // namespace extensions::theory::parallel::koggestone

#endif  // EXTENSIONS_THEORY_PARALLEL_KOGGESTONE_H
