#pragma once

#include <memory>

namespace taskloaf {

struct context_internals {
    virtual ~context_internals() {};
};
struct context {
    std::unique_ptr<context_internals> internals;
    context(std::unique_ptr<context_internals> internals);
    ~context();
    context(context&&);
};

struct config {
    bool print_stats = false;
};

context launch_local(size_t n_workers, config cfg = config());

#ifdef MPI_FOUND
context launch_mpi(config cfg = config());
#endif

} //end namespace taskloaf
