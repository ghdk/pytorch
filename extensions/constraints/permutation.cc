#include "../system.h"
#include "../iter/generated_iterator.h"
using namespace extensions::iter;
#include "accessor.h"
#include "permutation.h"
#pragma message __FILE__

// should be used as a functor to generated iterator
// constructor receives 1) a state, 2) domains, 3) constraints, where
// 1) a state is a tensor one dim
// 2) domains is a tensor, two dim/matrix, one row with the domain of each variable
// 3) constraints is a function FIXME API bool call(state, depth)
//    - a list of functions can be handled in python all()
//    - if we need network, define adj matrix using the indexes of the state,
//      and use that to drive the implementation of the constraints
//    - if we need performance implement them in c++
//



PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    using namespace extensions::constraints;
    auto c = py::class_<Permutation, extensions::ptr_t<Permutation>>(m, "Permutation");
    Permutation::def(c);
    m.def("cxxfoo", cxxfoo);
    m.def("cxxfoo2", cxxfoo2);
 }
