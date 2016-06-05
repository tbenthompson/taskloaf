#pragma once

#include <memory>

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

struct Config {
    bool print_stats = false;
};

Context launch_local(size_t n_workers, Config cfg = Config());

#ifdef MPI_FOUND
Context launch_mpi();
#endif

} //end namespace taskloaf
