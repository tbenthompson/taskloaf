#pragma once

#include "global_ref.hpp"

#include <functional>

namespace taskloaf {

struct ContextInternals {
    virtual ~ContextInternals() {};
};
struct Context {
    std::unique_ptr<ContextInternals> internals;
    Context(std::unique_ptr<ContextInternals> internals);
    ~Context();
    Context(Context&&);
};

// void launch_local_serializing(size_t n_workers, std::function<void()> f);
// void launch_local_singlethread(size_t n_workers, std::function<void()> f);
// void launch_local(size_t n_workers, std::function<void()> f);
Context launch_local(size_t n_workers);

#ifdef MPI_FOUND
Context launch_mpi();
#endif

} //end namespace taskloaf
