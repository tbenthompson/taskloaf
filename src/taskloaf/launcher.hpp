#pragma once

#include "ivar.hpp"

#include <functional>

namespace taskloaf {

template <typename F, typename Launcher>
void local_launch_coordinator(int n_workers, F&& f, Launcher launcher_fnc) {
    launcher_fnc(n_workers, [f = std::forward<F>(f)] () {
        auto t = ready(f()).unwrap();
        return t.ivar;
    });
}

void launch_local_helper_serializing(size_t n_workers, std::function<IVarRef()> f);

template <typename F>
void launch_local_serializing(int n_workers, F&& f) {
    local_launch_coordinator(n_workers, f, launch_local_helper_serializing);
}

void launch_local_helper_singlethread(size_t n_workers, std::function<IVarRef()> f);

template <typename F>
void launch_local_singlethread(int n_workers, F&& f) {
    local_launch_coordinator(n_workers, f, launch_local_helper_singlethread);
}

void launch_local_helper(size_t n_workers, std::function<IVarRef()> f);

template <typename F>
void launch_local(int n_workers, F&& f) {
    local_launch_coordinator(n_workers, f, launch_local_helper);
}

int shutdown();

#ifdef MPI_FOUND
void launch_mpi_helper(std::function<IVarRef()> f);

template <typename F>
void launch_mpi(F&& f) {
    launch_mpi_helper([f = std::forward<F>(f)] () {
        auto t = ready(f()).unwrap();
        return t.ivar;
    });
}
#endif

} //end namespace taskloaf
