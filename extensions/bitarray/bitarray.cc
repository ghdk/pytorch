/**
 * In contrast to /pytorch/c10/util/Bitset.h, it uses a tensor of dimension 1
 * to represent the bitarray.
 */

#include "../system.h"
using namespace extensions;
#include "bitarray.h"
using namespace extensions::bitarray;

accessor_t extensions::bitarray::set(accessor_t accessor, int64_t index, bool truth)
{
    assertm(size(accessor) > index && index >= 0, "Index ", index, " is out of bounds [0,", size(accessor), ").");
    auto i = index / cellsize;
    auto j = index % cellsize;
         j =         cellsize - 1 - j;

    cell_t mask = 0x1 << j;
    if(truth)
    {
        cell_t value = accessor[i];
        value |= mask;
        accessor[i] = value;
    }
    else
    {
        mask = ~mask;
        cell_t value = accessor[i];
        value &= mask;
        accessor[i] = value;
    }
    return accessor;
}

bool extensions::bitarray::get(accessor_t accessor, int64_t index)
{
    assertm(size(accessor) > index && index >= 0, "Index ", index, " is out of bounds [0,", size(accessor), ").");
    auto i = index / cellsize;
    auto j = index % cellsize;
         j =         cellsize - 1 - j;

    cell_t mask = 0x1 << j;
    cell_t value = accessor[i];
    return 0 != (value & mask);
}

accessor_t extensions::bitarray::negate(accessor_t accessor)
{
    assertm(size(accessor) > 0, "");
    for(int64_t i = 0; i < accessor.size(0); i++)
        accessor[i] = ~accessor[i];
    return accessor;
}

int64_t extensions::bitarray::size(accessor_t accessor)
{
    return accessor.size(0) * cellsize;
}

torch::Tensor extensions::bitarray::set(torch::Tensor tensor, int64_t index, bool truth)
{
    assertm(size(tensor) > index && index >= 0, "Index ", index, " is out of bounds [0,", size(tensor), ").");
    assert(tensor.is_contiguous());
    auto acc = tensor.accessor<cell_t, 1UL>();
    set(acc, index, truth);
    return tensor;
}

bool extensions::bitarray::get(torch::Tensor tensor, int64_t index)
{
    assertm(size(tensor) > index && index >= 0, "Index ", index, " is out of bounds [0,", size(tensor), ").");
    assert(tensor.is_contiguous());
    auto acc = tensor.accessor<cell_t, 1UL>();
    return get(acc, index);
}

torch::Tensor extensions::bitarray::negate(torch::Tensor tensor)
{
    assertm(size(tensor) > 0, "");
    assert(tensor.is_contiguous());
    auto acc = tensor.accessor<cell_t, 1UL>();
    negate(acc);
    return tensor;
}

int64_t extensions::bitarray::size(torch::Tensor tensor)
{
    assertm(1 == tensor.dim(), "Rank = " + std::to_string(tensor.dim()) + ".");
    assertm(torch::kUInt8 == tensor.dtype(), "");
    assert(tensor.is_contiguous());
    auto acc = tensor.accessor<cell_t, 1UL>();
    return size(acc);
}

void PYBIND11_MODULE_IMPL(py::module_ m)
{
    m.attr("CELL_SIZE") = pybind11::int_(bitarray::cellsize);
    m.def("set", static_cast<torch::Tensor(*)(torch::Tensor,int64_t,bool)>(&set));
    m.def("get", static_cast<bool(*)(torch::Tensor, int64_t)>(&get));
    m.def("negate", static_cast<torch::Tensor(*)(torch::Tensor)>(&negate));
    m.def("size", static_cast<int64_t(*)(torch::Tensor)>(&size));
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    PYBIND11_MODULE_IMPL(m);
}
