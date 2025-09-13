#ifndef EXTENSIONS_THEORY_PARALLEL_SORT_H
#define EXTENSIONS_THEORY_PARALLEL_SORT_H

namespace extensions { namespace theory { namespace parallel { namespace sort
{

void bitonic(torch::Tensor ib, torch::Tensor ob)
{
    torch::Tensor sb = extensions::theory::parallel::memmodel::shared_memory(ib.options());

    auto delegate = [](auto ia, auto oa, auto sa)
    {

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

}}}}  // namespace extensions::theory::parallel::sort

#endif  // EXTENSIONS_THEORY_PARALLEL_SORT_H
