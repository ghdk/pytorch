#ifndef EXTENSIONS_BITARRAY_BITARRAY_H
#define EXTENSIONS_BITARRAY_BITARRAY_H

namespace extensions { namespace bitarray
{

using cell_t = c10::impl::ScalarTypeToCPPTypeT<torch::kUInt8>;
constexpr int64_t cellsize = sizeof(cell_t) * CHAR_BIT;
using accessor_t = at::TensorAccessor<cell_t, 1UL>;

EXPORT accessor_t set(accessor_t accessor, int64_t index, bool truth);
EXPORT accessor_t negate(accessor_t accessor);
EXPORT bool get(accessor_t accessor, int64_t index);
EXPORT int64_t size(accessor_t accessor);

EXPORT torch::Tensor set(torch::Tensor tensor, int64_t index, bool truth);
EXPORT torch::Tensor negate(torch::Tensor tensor);
EXPORT bool get(torch::Tensor tensor, int64_t index);
EXPORT int64_t size(torch::Tensor tensor);  // number of bits
    
}}  // namespace extensions::bitarray

#endif  // EXTENSIONS_BITARRAY_BITARRAY_H
