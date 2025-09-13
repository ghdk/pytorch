#ifndef EXTENSIONS_THEORY_PARALLEL_PARALLEL_H
#define EXTENSIONS_THEORY_PARALLEL_PARALLEL_H

namespace extensions { namespace theory { namespace parallel
{

template<typename H, size_t N, size_t M>
void copy(torch::TensorAccessor<H,N> ib, torch::TensorAccessor<H,M> ob, size_t offset_ib, size_t offset_ob)
{
    int64_t grain_sz = ob.size(0) / extensions::theory::parallel::memmodel::pool_sz;
    auto kernel = [&ib, &ob, &offset_ib, &offset_ob](int64_t begin, int64_t end)
    {
        for(int64_t i = begin; i < end; ++i)
        {
            int64_t index_ib = i + offset_ib;
            int64_t index_ob = i + offset_ob;
            if(index_ib >= ib.size(0)) break;
            if(index_ob >= ob.size(0)) break;
            ob[index_ob] = ib[index_ib];
        }
    };

    at::parallel_for(0, ob.size(0), grain_sz, kernel);
}

void copy(torch::Tensor ib, torch::Tensor ob, size_t offset_ib, size_t offset_ob)
{
    if(ib.dtype() == torch::kInt64)
    {
        copy(ib.accessor<int64_t, 1UL>(), ob.accessor<int64_t, 1UL>(), offset_ib, offset_ob);
    }
    if(ib.dtype() == torch::kFloat)
    {
        copy(ib.accessor<float, 1UL>(), ob.accessor<float, 1UL>(), offset_ib, offset_ob);
    }
}

void fanout(std::function<void(int64_t begin, int64_t end)> func)
{
    at::parallel_for(0, extensions::theory::parallel::memmodel::pool_sz, 1, func);
}

}}}  // namespace extensions::theory::parallel

#endif  // EXTENSIONS_THEORY_PARALLEL_PARALLEL_H
