#ifndef EXTENSIONS_THEORY_PARALLEL_MEMMODEL_H
#define EXTENSIONS_THEORY_PARALLEL_MEMMODEL_H

namespace extensions { namespace theory { namespace parallel { namespace memmodel
{

/**
 * In CUDA threads are organised into blocks, with the blockIdx
 * holding the index of the block. Hence in a flattened indexing scheme
 * each thread has the index=blockIdx.x * blockDim.x + threadIdx.x.
 * Blocks are assigned to streaming multiprocessors (SMs), and split into
 * warps of equal size (HW specific) which are the scheduling units.
 *
 * Instead of having a hierarchy of blocks and threads per block,
 * we have a flat indexing scheme.
 *
 * In terms of the memory layout, a) the global memory corresponds to
 * the input and output tensors, b) the shared memory is a tensor of
 * size cpu count x cpu count. This allows us to emulate a shared
 * memory that is smaller than the global memory, but big enough
 * to offer a region to each thread.
 */

static const int64_t pool_sz = at::get_num_threads();
static const int64_t tile_sz = pool_sz;

static torch::Tensor shared_memory(torch::TensorOptions inherit)
{
    return torch::zeros({tile_sz * tile_sz}, inherit);
}

/**
 * ARM CPUs seem to have a weak memory ordering model, see
 * Arm Architecture Reference Manual Armv8, for Armv8-A architecture profile,
 * hence a hardware memory barrier is required.
 *
 * https://www.cl.cam.ac.uk/~pes20/cpp/cpp0xmappings.html
 */

#define MEMORY_BARRIER(void)  std::atomic_thread_fence(std::memory_order_release)

/**
 * A barrier implementation that asserts when a thread waits for >= 1 sec.
 * Remember that the thread index is different from thread id and the thread
 * number in the trace of MacOS. Due to the Aten/Parallel API we have no control
 * over which thread the kernel is executed.
 */
struct Barrier
{
    using array_t = std::array<size_t, 64>;

    static void wait(int64_t thread_idx)
    {
        assertm((size_t)thread_idx < barrier_.size(), thread_idx, barrier_.size());

        // we add the thread idx to the timeout to create an ordering of the
        // asserts, increasing the probability that only one will fire
        const int64_t timeout = 1000 - barrier_.size();  // ie. at most one second

        auto temp = barrier_[thread_idx] + 1;

        {
            auto start = std::chrono::high_resolution_clock::now();
            while(thread_idx && barrier_[thread_idx - 1] != temp)
            {
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                assertm(duration.count() < timeout + thread_idx, dump(barrier_));
            }
        }

        MEMORY_BARRIER();
        barrier_[thread_idx] = temp;

        {
            auto start = std::chrono::high_resolution_clock::now();
            while(barrier_[thread_idx] != barrier_[pool_sz - 1])
            {
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                assertm(duration.count() < timeout + thread_idx, dump(barrier_));
            }
        }
    }

private:
    static array_t barrier_;
};
Barrier::array_t Barrier::barrier_ = {0};

}}}}  // namespace extensions::theory::parallel::memmodel

#endif  // EXTENSIONS_THEORY_PARALLEL_MEMMODEL_H
