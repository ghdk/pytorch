#ifndef EXTENSIONS_CONSTRAINTS_FORWARD_CHECKING_H
#define EXTENSIONS_CONSTRAINTS_FORWARD_CHECKING_H

namespace extensions { namespace constraints
{

class ForwardChecking
{
    /**
     * We implement forward checking through a constraint functor,
     * instead of embedding it in the solver. The following variable
     * is a 2d tensor, corresponding to domain::Domain. When we
     * assign a value to a variable n, we apply the constraint
     * to variables >n, lets call them nn. When the constraint fails
     * for a value i from the domain of a variable nn, we set the
     * domain_prunning[nn][i] = n if n is the most shallow variable
     * that caused a conflict.
     *
     * See constraints_mutex.h for an example on the population of
     * the domain prunning set.
     */

    bool operator()(int64_t depth, int64_t domain_index)
    {
        bool ret = true;
        if((*domain_prunning_acc_)[depth][domain_index] < depth)
            ret = false;
        return ret;
    }

public:
    ForwardChecking(torch::Tensor domain_prunning)
    : domain_prunning_{std::move(domain_prunning)}
    {
        assertm(2 == domain_prunning_.dim(), "Rank = " + std::to_string(domain_prunning_.dim()) + ".");

        if(domain_prunning_.dim())
            domain_prunning_acc_ = domain_prunning_.accessor<int64_t, 2UL>();
    }

private:
    // a 2d tensor of size NxM, where N is the number of variables in the
    // state. Each row is of length M, and consists of
    // row[0],row[1],...,row[m],...,row[M-1], where
    // row[0] holds the number of values in the domain, ie. m,
    // row[1],...,row[m], holds the values,
    // row[m+1],...,row[M-1], undefined
    torch::Tensor domain_prunning_;
    domain_prunning_accessor_t domain_prunning_acc_;

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
#endif
};

}}  // namespace extensions::constraints

#endif  // EXTENSIONS_CONSTRAINTS_FORWARD_CHECKING_H
