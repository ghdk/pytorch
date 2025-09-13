#ifndef EXTENSIONS_RSSE_THREADPOOL_H
#define EXTENSIONS_RSSE_THREADPOOL_H

namespace extensions { namespace rsse
{

/**
 * We don't need to map jobs to rings, to require that rings are long enough
 * to hold all pending jobs. If a producer finds a ring to be full, it can
 * move on to the next thread. Also we don't need the producer to remain
 * unblocked, if we cannot find a ring we can loop until we do.
 *
 * If we need to implement a hashing function for distributing work to threads,
 * we can post the same function to every ring, and each thread can execute
 * the part specified by the hashing algorithm.
 *
 * What is the shortest ring size required to guarantee
 * that the threads wont get starved?
 *
 * Having t threads, the producer needs to be >=t times faster than each thread,
 * to guarantee that each thread will always have a pending job. Hence if
 * the execution of the producer had real-time guarantees, we would need rings
 * of size 2.
 */

namespace policy
{
/**
 * broadcast: async() posts the same job on every ring.
 * random: async() posts the job to the next, randomly
 * chosen thread with a non-full ring.
 */
class broadcast{};
class random{};

inline constexpr broadcast bcasync {};
inline constexpr random rndasync {};
}

class Kernel
{
public:
    virtual ~Kernel(){}
    virtual int operator()(size_t) = 0;
};

/**
 * The size of the ring has the following properties:
 * - Smaller rings result in higher probability that the producer thread will
 * block.
 * - Larger rings result in higher probability that some of the rings will be
 * empty due to the random generator.
 * - Ring sizes should be >2, to lower the probability of threads starving.
 */
struct alignas(hardware_destructive_interference_size_double) Ring
{
public:
    using index_t = uint8_t;
    using func_t = ptr_t<Kernel>;
    static constexpr size_t ring_size = int((hardware_destructive_interference_size_double - 3 * sizeof(index_t)) / sizeof(func_t));
    using funcset_t = std::array<func_t, ring_size>;

public:
    Ring()
    : consumer_{0}
    , producer_{0}
    , active_{true}
    , ring_{}
    {}
    Ring(Ring const& other) = delete;
    Ring(Ring&& other) = default;
    Ring& operator=(Ring const& other) = delete;
    Ring& operator=(Ring&& other) = delete;
public:
    void push(func_t);
    void visit(size_t);
    bool empty() const;
    bool full() const;
public:
    index_t consumer_;
    index_t producer_;
    index_t active_;
    funcset_t ring_;
};

static_assert(hardware_destructive_interference_size_double >= sizeof(Ring));

class ThreadPool
{
public:
    using ringset_t = std::vector<Ring>;
    using threadset_t = std::vector<std::thread>;
    using distribution_t = std::uniform_int_distribution<size_t>;
public:
    ThreadPool() noexcept;
    ~ThreadPool();
public:
    int async(policy::broadcast, typename Ring::func_t);
    int async(policy::random, typename Ring::func_t);
    void join();
    void wait() const;
    size_t size() const;
private:
    static void run(Ring&, size_t);
private:
    ringset_t rings_;
    threadset_t threads_;
private:
    std::random_device rnd_dev_;
    std::default_random_engine rnd_gen_;
    distribution_t distribution_;

#ifdef PYDEF
    public:
        template <typename PY>
        static PY def(PY& c)
        {
            using T = typename PY::type;

            c.def(py::init<>());

            c.def("bcasync",
                  +[](ptr_t<ThreadPool> self, ptr_t<Kernel> f)
                  {
                        return self->async(policy::bcasync, f);
                  },
                  py::call_guard<py::gil_scoped_release>());
            c.def("rndasync",
                  +[](ptr_t<ThreadPool> self, ptr_t<Kernel> f)
                  {
                        return self->async(policy::rndasync, f);
                  },
                  py::call_guard<py::gil_scoped_release>());

            /**
             * When Python code calls C++, and vice versa, the GIL is held.
             * Hence if we call wait or join from Python, the call will acquire
             * the GIL, preventing any of the threads from calling Python
             * functions. Hence, if the rings hold Kernel objects that are
             * implemented in Python, by calling wait or join from Python we
             * effectively deadlock.
             *
             * See https://pybind11.readthedocs.io/en/stable/advanced/misc.html#global-interpreter-lock-gil
             */
            c.def("join", &T::join, py::call_guard<py::gil_scoped_release>());
            c.def("wait", &T::wait, py::call_guard<py::gil_scoped_release>());
            c.def("size", &T::size);
            return c;
        }
#endif  // PYDEF
};

/**
 * On exposing pure virtual classes the pybind11 documentation suggests that
 * a trampoline object is used, see
 * https://pybind11.readthedocs.io/en/stable/advanced/classes.html .
 * However, since we are passing shared_ptr objects between C++ and Python,
 * the lifetime of the trampoline objects does not span the lifetime of the
 * shared_ptr objects. To resolve this issue, we treat the Python objects as
 * callables, which we wrap in PyKernel, and we push the PyKernel in the
 * rings.
 */
class PyKernel: public Kernel
{
public:
    using func_t = std::function<int(size_t)>;

    PyKernel(func_t f)
    : f_{f}
    {}

    int operator()(size_t thread) override
    {
        return f_(thread);
    }

    func_t f_;

#ifdef PYDEF
public:
    template <typename PY>
    static PY def(PY& c)
    {
        using T = typename PY::type;

        c.def(py::init<typename T::func_t>());
        c.def("__call__", &T::operator());
        return c;
    }
#endif  // PYDEF
};

}}  // namespace extensions::rsse

#endif  // EXTENSIONS_RSSE_THREADPOOL_H
