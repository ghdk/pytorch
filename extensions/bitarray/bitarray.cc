/**
 * In contrast to /pytorch/c10/util/Bitset.h, it uses a tensor of dimension 1
 * to represent the bitarray.
 */

#include "../system.h"
#include "bitarray.h"

// FIXME tensor.accessor overload

torch::Tensor& extensions::bitarray::set(torch::Tensor& tensor, size_t index, bool truth)
{
    assertm(1 == tensor.dim(), "Rank = " + std::to_string(tensor.dim()) + ".");
    assertm(torch::kUInt8 == tensor.dtype(), "");
    
    auto i = index / (sizeof(cell_t) * CHAR_BIT);
    auto j = index % (sizeof(cell_t) * CHAR_BIT);
         j =         (sizeof(cell_t) * CHAR_BIT) - 1 - j;

    cell_t mask = 0x1 << j;
    if(truth)
    {
        cell_t value = tensor[i].item<cell_t>();
        value |= mask;
        tensor[i] = value;
    }
    else
    {
        mask = ~mask;
        cell_t value = tensor[i].item<cell_t>();
        value &= mask;
        tensor[i] = value;
    }
    return tensor;
}

torch::Tensor extensions::bitarray::get(torch::Tensor const& tensor, size_t index)
{
    assertm(1 == tensor.dim(), "Rank = " + std::to_string(tensor.dim()) + ".");
    assertm(torch::kUInt8 == tensor.dtype(), "");

    auto i = index / (sizeof(cell_t) * CHAR_BIT);
    auto j = index % (sizeof(cell_t) * CHAR_BIT);
         j =         (sizeof(cell_t) * CHAR_BIT) - 1 - j;

    cell_t mask = 0x1 << j;
    cell_t value = tensor[i].item<cell_t>();
    return torch::tensor({0 != (value & mask)});
}

torch::Tensor& extensions::bitarray::negate(torch::Tensor& tensor)
{
    assertm(1 == tensor.dim(), "Rank = " + std::to_string(tensor.dim()) + ".");
    assertm(torch::kUInt8 == tensor.dtype(), "");

    size_t count = static_cast<size_t>(tensor.sizes()[0]);
    for(size_t i = 0; i < count; i++)
        tensor[i] = ~tensor[i].item<cell_t>();
    return tensor;
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    m.def("set", &extensions::bitarray::set);
    m.def("get", &extensions::bitarray::get);
    m.def("negate", &extensions::bitarray::negate);
}
