#ifndef EXTENSIONS_CONSTRAINTS_CONSTRAINT_MUTEX_H
#define EXTENSIONS_CONSTRAINTS_CONSTRAINT_MUTEX_H

namespace extensions { namespace constraints
{

class ConstraintMutex
{
    bool operator()(int64_t depth, int64_t domain_index)
    {
        bool ret = true;
        if(0 < depth)
        {
            int64_t i = depth-1;
            ret = ((*state_acc_)[i] != (*state_acc_)[depth]);
            if(!ret && conflict_sets_.dim())
                (*conflict_sets_acc_)[state_size_ * depth + i] = 1;
        }
        if(domain_prunning_.dim() && depth+1 < state_size_)
        {
            int64_t i = depth+1;
            float assignment = (*domains_acc_)[depth][domain_index];
            int64_t domain_size = (*domains_acc_)[i][0];
            for(int64_t d = 1; d <= domain_size; d++)
            {
                if(assignment == (*domains_acc_)[i][d])  // constraint
                {
                    if(depth < (*domain_prunning_acc_)[i][d])
                        (*domain_prunning_acc_)[i][d] = depth;
                }
                else
                {
                    if(depth == (*domain_prunning_acc_)[i][d])
                    {
                        // we have backtracked and we are trying another
                        // value for the variable at depth, hence reset
                        // the pruning
                        (*domain_prunning_acc_)[i][d] = state_size_;
                    }
                }
            }
        }
        return ret;
    }

public:
    ConstraintMutex(torch::Tensor state, torch::Tensor domains,
                    torch::Tensor conflict_sets, torch::Tensor domain_prunning)
    : state_{std::move(state)}
    , domains_{std::move(domains)}
    , conflict_sets_{std::move(conflict_sets)}
    , domain_prunning_{std::move(domain_prunning)}
    {
        state_size_ = static_cast<int64_t>(state_.sizes()[0]);
        state_acc_ = state_.accessor<float,1UL>();
        if(domains_.dim())
            domains_acc_ = domains_.accessor<float, 2UL>();
        if(conflict_sets_.dim())
            conflict_sets_acc_ = conflict_sets_.accessor<int64_t, 1UL>();
        if(domain_prunning_.dim())
            domain_prunning_acc_ = domain_prunning_.accessor<int64_t, 2UL>();
    }
private:
    torch::Tensor state_;
    state_accessor_t state_acc_;
    int64_t state_size_;
    torch::Tensor domains_;
    domains_accessor_t domains_acc_;
    torch::Tensor conflict_sets_;  // see conflict directed backjumping
    conflict_sets_accessor_t conflict_sets_acc_;
    torch::Tensor domain_prunning_;  // see forward checking
    domain_prunning_accessor_t domain_prunning_acc_;

#ifdef PYDEF
public:
    template <typename PY>
    static PY def(PY& c)
    {
        using T = typename PY::type;
        c.def(py::init<torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor>());
        c.def("__call__", &T::operator());
        return c;
    }
#endif  // PYDEF
};

}}  // namespace extensions::constraints

#endif  // EXTENSIONS_CONSTRAINTS_CONSTRAINT_MUTEX_H
