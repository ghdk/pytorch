#ifndef EXTENSIONS_CONSTRAINTS_PERMUTATION_H
#define EXTENSIONS_CONSTRAINTS_PERMUTATION_H

namespace extensions { namespace iter { template<typename ValueType> class GeneratedIterator; }}

namespace extensions { namespace constraints
{

bool cxxfoo(torch::Tensor& tensor, size_t depth, size_t domain)
{
    tensor[depth] = (double)domain;
    return true;
}

std::pair<torch::Tensor, bool> cxxfoo2(void)
{
    auto options = torch::TensorOptions().dtype(torch::kInt64)
                                         .requires_grad(false);
    auto ret = torch::zeros({10}, options);
    return {ret,true};
}

class Permutation
{
private:  // typedefs
    using ret_t = GeneratedIterator<torch::Tensor>::generator_t::result_type;
public:
    using constraint_t = std::function<bool(int64_t, int64_t)>;

    // FIXME optional for return value

    ret_t operator()(void)
    {

//        std::cerr << "depth=" << depth_ << std::endl;

        while(depth_ >= 0)
        {
            if(depth_ >= max_depth_)
            {
                depth_ -= 1;
//                std::cerr << "true" << std::endl;
                return {state_};
            }
            else
            {
                if((*domain_indexes_acc_)[depth_] <= (*domains_acc_)[depth_][0])
                {
                    int64_t domain_index = (*domain_indexes_acc_)[depth_];
                    (*state_acc_)[depth_] = (*domains_acc_)[depth_][domain_index];
                    (*domain_indexes_acc_)[depth_] += 1;
                    if(std::all_of(constraints_.begin(), constraints_.end(), [this, domain_index](constraint_t const& c){return c(this->depth_, domain_index);}))
                        depth_ += 1;
                }
                else  // we need to backtrack
                {
                    (*domain_indexes_acc_)[depth_] = 1;
                    depth_ -= 1;
                }
            }
        }
//        std::cerr << "false" << std::endl;
        return std::nullopt;
    }

    Permutation(torch::Tensor state, torch::Tensor domains, std::vector<constraint_t> constraints)
    : state_{std::move(state)}
    , domains_{std::move(domains)}
    , constraints_{std::move(constraints)}
    {
        depth_ = 0;
        max_depth_ = static_cast<int64_t>(state_.sizes()[0]);
        std::cerr << "md=" << max_depth_ << std::endl;
        auto options = torch::TensorOptions().dtype(torch::kInt64)
                                             .requires_grad(false);
        domain_indexes_ = torch::ones({max_depth_}, options);
        std::cerr << "di=" << domain_indexes_.sizes()[0] << std::endl;

        state_acc_ = state_.accessor<float,1UL>();
        domains_acc_ = domains_.accessor<float,2UL>();
        domain_indexes_acc_ = domain_indexes_.accessor<int64_t,1UL>();

    }

private:
    torch::Tensor state_;  // a 1d tensor
    state_accessor_t state_acc_;
    // a 2d tensor of size NxM, where N is the number of variables in the
    // state. Each row is of length M, and consists of
    // row[0],row[1],...,row[m],...,row[M-1], where
    // row[0] holds the number of values in the domain, ie. m,
    // row[1],...,row[m], holds the values,
    // row[m+1],...,row[M-1], undefined
    torch::Tensor const domains_;
    domains_accessor_t domains_acc_;
    std::vector<constraint_t> const constraints_;
    int64_t max_depth_;
    int64_t depth_;
    torch::Tensor domain_indexes_;  // a 1d tensor
    index_accessor_t domain_indexes_acc_;

#ifdef PYDEF
public:
    template <typename PY>
    static PY def(PY& c)
    {
        using T = typename PY::type;
        c.def(py::init<torch::Tensor, torch::Tensor, std::vector<constraint_t>>());
        c.def("__call__", &T::operator());
        return c;
    }
#endif  // PYDEF
};

}}  // namespace extensions::constraints

#endif  // EXTENSIONS_CONSTRAINTS_PERMUTATION_H
