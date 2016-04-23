#pragma once

#include "ivar.hpp"

#include <functional>

namespace taskloaf {

void launch_local_serializing(size_t n_workers, std::function<void()> f);
void launch_local_singlethread(size_t n_workers, std::function<void()> f);
void launch_local(size_t n_workers, std::function<void()> f);

int shutdown();

#ifdef MPI_FOUND
void launch_mpi(std::function<void()> f);
#endif

} //end namespace taskloaf
