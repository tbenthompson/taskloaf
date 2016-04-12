#pragma once

#include "ivar.hpp"

#include <functional>

namespace taskloaf {

void launch_local_helper(size_t n_workers, std::function<IVarRef()> f);
void launch_local_helper_serializing(size_t n_workers, std::function<IVarRef()> f);
void launch_local_helper_singlethread(size_t n_workers, std::function<IVarRef()> f);

template <typename F, typename Launcher>
void launch(int n_workers, F&& f, Launcher launcher_fnc) {
    launcher_fnc(n_workers, [f = std::forward<F>(f)] () {
        auto t = ready(f()).unwrap();
        return t.ivar;
    });
}

template <typename F>
void launch_local_serializing(int n_workers, F&& f) {
    launch(n_workers, f, launch_local_helper_serializing);
}

template <typename F>
void launch_local_singlethread(int n_workers, F&& f) {
    launch(n_workers, f, launch_local_helper_singlethread);
}

template <typename F>
void launch_local(int n_workers, F&& f) {
    launch(n_workers, f, launch_local_helper);
}

int shutdown();

} //end namespace taskloaf
