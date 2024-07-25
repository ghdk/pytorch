#ifndef EXTENSIONS_CONSTRAINTS_CONFLICT_DIRECTED_BACKJUMPING_H
#define EXTENSIONS_CONSTRAINTS_CONFLICT_DIRECTED_BACKJUMPING_H

namespace extensions { namespace constraints
{

// FIXME bitmap for conflict set

class ConflictDirectedBackjumping
{

    /**
     * The conflict set is shared among all the constraints that want to
     * implement CBJ. When a constraint fails against an assignment of a
     * variable i, it marks the truth value of conflict_set[depth][i],
     * translated to the index length(state) * depth + i, to true.
     *
     * Instead of implementing CBJ as part of the search algorithm, we implement
     * it as a constraint, forcing the algorithm to backtrack further until the
     * backjumping point is reached.
     *
     * See constraint_mutex.h for an example on the population of the conflict
     * set.
     */

    bool operator()(int64_t depth, int64_t domain_index)
    {
        // 1046166
        static int64_t backjumping_point = -1;
        bool ret = true;
        if(!conflict_sets_.dim()) return ret;
        if(depth > depth_)  // search moves deeper
            depth_ = depth;
        if(depth == depth_)  // search iterates over the domain
            depth_ = depth;
        if(depth < depth_)  // search backtracks
        {
            if(0 > backjumping_point)
            {
                backjumping_point = depth_;
                while(!(*conflict_sets_acc_)[state_size_ * depth_ + backjumping_point])
                {
                    backjumping_point -= 1;
                    if(0 > backjumping_point) break;
                }
            }
            if(0 <= backjumping_point)
            {
                if(depth > backjumping_point)
                    ret = false;  // continue backtracking
                else  // depth <= backjumping point
                {
                    // depth should be equal to the backjumping_point,
                    // hence propagate the conflict set
                    for(int64_t i = 0; i < state_size_; i++)
                    {
                        (*conflict_sets_acc_)[state_size_ * backjumping_point + i] = (((*conflict_sets_acc_)[state_size_ * backjumping_point + i]
                                                                                       || (*conflict_sets_acc_)[state_size_ * depth_ + i])
                                                                                      && i != backjumping_point);
                    }
                    // reset the conflict sets for depth < i <= depth_
                    // where depth_ is the depth we backjump from.
                    for(int64_t i = depth+1; i <= depth_; i++)
                        for(int64_t j = 0; j < state_size_; j++)
                            (*conflict_sets_acc_)[state_size_ * i + j] = 0;
                    depth_ = depth;
                    backjumping_point = -1;
                }
            }
            else  // there is no backjumping point, switch to backtracking
                depth_ = depth;
        }
        return ret;
    }

public:
    ConflictDirectedBackjumping(torch::Tensor conflict_sets)
    : state_size_{0}
    , conflict_sets_{std::move(conflict_sets)}
    , depth_{0}
    {
        assertm(1 >= conflict_sets_.dim(), "Rank = " + std::to_string(conflict_sets_.dim()) + ".");
        assertm(torch::kInt64 == conflict_sets_.dtype());

        if(conflict_sets_.dim())
        {
            state_size_ = sqrt(static_cast<int64_t>(conflict_sets_.sizes()[0]));
            conflict_sets_acc_ = conflict_sets_.accessor<int64_t, 1UL>();
        }
    }

private:
    int64_t state_size_;
    // A 1D tensor holding a bitmap of size NxN,
    // where N is the number of variables in the state.
    torch::Tensor conflict_sets_;
    conflict_sets_accessor_t conflict_sets_acc_;
    int64_t depth_;

#ifdef PYDEF
public:
    template <typename PY>
    static PY def(PY& c)
    {
        using T = typename PY::type;
        c.def(py::init<torch::Tensor>());
        c.def("__call__", &T::operator());
        return c;
    }
#endif  // PYDEF
};

}}  // namespace extensions::constraints

#endif  // EXTENSIONS_CONSTRAINTS_CONFLICT_DIRECTED_BACKJUMPING_H
