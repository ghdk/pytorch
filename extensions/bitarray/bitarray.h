#ifndef EXTENSIONS_BITARRAY_BITARRAY_H
#define EXTENSIONS_BITARRAY_BITARRAY_H

namespace extensions { namespace bitarray
{

using cell_t = uint8_t;

torch::Tensor& set(torch::Tensor& tensor, size_t index, bool truth);
torch::Tensor get(torch::Tensor const& tensor, size_t index);
torch::Tensor& negate(torch::Tensor& tensor);
    
}}  // namespace extensions::bitarray

#endif  // EXTENSIONS_BITARRAY_BITARRAY_H
