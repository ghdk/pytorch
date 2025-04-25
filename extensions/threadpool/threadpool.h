#ifndef EXTENSIONS_RSSE_THREADPOOL_H
#define EXTENSIONS_RSSE_THREADPOOL_H

namespace extensions { namespace rsse
{

/**
 * Imitates the GPU parallelism model. The threadpool defines a block of
 * threads. A kernel is posted on every ring, and executed by every thread.
 * Each thread passes the thread index to the kernel. A single thread owns the
 * threadpool. The threads of the treadpool cannot launch new kernels.
 *
 * The memory model is also similar to the GPU model. The kernels use a shared
 * buffer for communication. The owner thread reads the shared buffer to collect
 * the results.
 */

class Kernel
{
public:
    virtual ~Kernel(){}
    virtual int operator()(size_t) = 0;
};

struct alignas(hardware_destructive_interference_size_double) Ring
{
public:
    using index_t = uint8_t;
    using func_t = ptr_t<Kernel>;
    static constexpr size_t ring_size = int((hardware_destructive_interference_size_double - 2 * sizeof(index_t)) / sizeof(func_t));
    using funcset_t = std::array<func_t, ring_size>;

public:
    Ring()
    : consumer_{0}
    , producer_{0}
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
    funcset_t ring_;
};

static_assert(hardware_destructive_interference_size_double >= sizeof(Ring));

class ThreadPool
{
public:
    using ringset_t = std::vector<Ring>;
    using threadset_t = std::vector<std::thread>;
public:
    ThreadPool() noexcept;
    ~ThreadPool();
public:
    void launch(typename Ring::func_t);
    void join();
    void wait() const;
    size_t size() const;
private:
    static void run(Ring&, bool&, size_t);
private:
    ringset_t rings_;
    threadset_t threads_;
    std::thread::id owner_;
    bool joined_;

#ifdef PYDEF
    public:
        template <typename PY>
        static PY def(PY& c)
        {
            using T = typename PY::type;

            c.def(py::init<>());

            c.def("launch",
                  +[](ptr_t<ThreadPool> self, ptr_t<Kernel> f)
                  {
                        return self->launch(f);
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
