#ifndef EXTENSIONS_CONSTRAINTS_ACCESSOR_H
#define EXTENSIONS_CONSTRAINTS_ACCESSOR_H

namespace extensions { namespace constraints
{

template <typename T, size_t N>
using accessor_t = std::optional<at::TensorAccessor<T, N>>;

using state_accessor_t = accessor_t<float, 1UL>;
using domains_accessor_t = accessor_t<float, 2UL>;
using index_accessor_t = accessor_t<int64_t, 1UL>;
using conflict_sets_accessor_t = accessor_t<int64_t, 1UL>;
using domain_prunning_accessor_t = accessor_t<int64_t, 2UL>;

}}  // namespace extensions::constraints

#endif  // EXTENSIONS_CONSTRAINTS_ACCESSOR_H
