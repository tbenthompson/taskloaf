#pragma once
#include "address.hpp"
#include "ivar.hpp"
#include "closure.hpp"

#include <memory>

namespace taskloaf { 

struct Worker {
    virtual ~Worker() {}

    virtual void shutdown() = 0;
    virtual bool can_compute_immediately() = 0;
    virtual size_t n_workers() const = 0;
    virtual void add_task(TaskT f) = 0;
    virtual void fulfill(const IVarRef& ivar, std::vector<Data> vals) = 0;
    virtual void add_trigger(const IVarRef& ivar, TriggerT trigger) = 0;
    virtual void dec_ref(const IVarRef& ivar) = 0;
};

extern thread_local Worker* cur_worker;

bool can_run_immediately();

template <typename F, typename... Args>
void add_task(F&& f, Args&&... args) {
    if (can_run_immediately()) {
        f(args...);
    } else {
        cur_worker->add_task(TaskT{
            [] (std::vector<Data>& args) {
                //TODO: Remove this copying
                std::vector<Data> non_fnc_args;
                for (size_t i = 1; i < args.size(); i++) {
                    non_fnc_args.push_back(args[i]);
                }
                apply_data_args(
                    args[0].get_as<typename std::decay<F>::type>(),
                    non_fnc_args
                );
            },
            std::vector<Data>{ 
                make_data(std::forward<F>(f)),
                make_data(std::forward<Args>(args))...
            }
        });
    }
}


} //end namespace taskloaf
