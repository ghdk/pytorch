#include "../system.h"
using namespace extensions;
#include "threadpool.h"
using namespace extensions::rsse;

/**
 * ARM CPUs seem to have a weak memory ordering model, see
 * Arm Architecture Reference Manual Armv8, for Armv8-A architecture profile,
 * hence a hardware memory barrier is required.
 *
 * https://www.cl.cam.ac.uk/~pes20/cpp/cpp0xmappings.html
 */

#define MEMORY_BARRIER(void)  std::atomic_thread_fence(std::memory_order_release)

void Ring::push(Ring::func_t f)
{
    assert(!full());
    ring_[producer_] = f;

    MEMORY_BARRIER();

    producer_ = (producer_ + 1) % ring_.size();
}

void Ring::visit(size_t thread)
{
    assert(!empty());
    auto f = ring_[consumer_];
    (*f)(thread);
    ring_[consumer_] = nullptr;

    MEMORY_BARRIER();

    /**
     * Advance the consumer after it has ran the kernel. As such a call to
     * wait will block until all kernels have ran to completion.
     */
    consumer_ = (consumer_ + 1) % ring_.size();
}

bool Ring::full() const
{
    return (producer_ + 1) % ring_.size() == consumer_;
}
bool Ring::empty() const
{
    return producer_ == consumer_;
}

ThreadPool::ThreadPool() noexcept
: rings_{}
, threads_{}
, owner_{std::this_thread::get_id()}
, joined_{false}
{
    size_t concurrency = std::max(1U, std::thread::hardware_concurrency() - 1);  // spawn at least 1 thread
    threads_.resize(concurrency);
    rings_.resize(concurrency);
    for(size_t i = 0; i < threads_.size(); ++i)
        threads_[i] = std::thread(ThreadPool::run, std::ref(rings_[i]), std::ref(joined_), i);
}

ThreadPool::~ThreadPool()
{
    join();
}

void ThreadPool::join()
{
    /**
     * It does not wait for the rings to become empty. If we want all kernels
     * to run before the join, we need to wait explicitly.
     */
    joined_ = true;

    MEMORY_BARRIER();

    for(auto& t : threads_)
        if(t.joinable())
            t.join();
}

void ThreadPool::wait() const
{
    assert(!joined_);  // catch use after join
    for(auto& r : rings_)
        while(!r.empty())
            continue;
}

size_t ThreadPool::size() const
{
    return threads_.size();
}

void ThreadPool::launch(typename Ring::func_t f)
{
    assert(!joined_);  // catch use after join
    auto source = std::this_thread::get_id();
    assertm(source == owner_, source, owner_);

    size_t rotation = 0;
    std::vector<bool> bitset(rings_.size(), false);
    while(true)
    {
        rotation = (rotation+1) % rings_.size();
        if(rings_[rotation].full()) continue;
        if(bitset[rotation]) continue;
        rings_[rotation].push(f);
        bitset[rotation] = true;
        if(std::all_of(bitset.begin(), bitset.end(), [](bool v){ return v; }))
            break;
    }
}

void ThreadPool::run(Ring& ring, bool& joined, size_t thread)
{
    while(!joined)
    {
        if(ring.empty()) continue;
        ring.visit(thread);
    }
}

// The IDE doesn't like the PYBIND11_MODULE macro, hence we redirect it
// to this function which is easier to read.
void PYBIND11_MODULE_IMPL(py::module_ m)
{

    {
        /**
         * Test submodule
         */

        auto mt = m.def_submodule("test", "");

        {
            auto c = py::class_<ThreadPool, ptr_t<ThreadPool>>(mt, "ThreadPool");
            ThreadPool::def(c);

            auto t = py::class_<Kernel, ptr_t<Kernel>>(mt, "KernelPureVirtual");

            auto f = py::class_<PyKernel, Kernel, ptr_t<PyKernel>>(mt, "Kernel");
            PyKernel::def(f);
        }

    }
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
    PYBIND11_MODULE_IMPL(m);
}
