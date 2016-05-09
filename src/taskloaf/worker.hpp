#pragma once
#include "address.hpp"
#include "ivar.hpp"
#include "closure.hpp"

#include <memory>

namespace taskloaf { 

struct Comm;
struct WorkerInternals;
struct Worker {
    std::unique_ptr<WorkerInternals> p;

    Worker(std::unique_ptr<Comm> comm);
    Worker(Worker&&) = default;
    ~Worker();

    bool can_compute_immediately() const;

    bool is_stopped() const;
    Comm& get_comm();
    const Address& get_addr();

    void shutdown();
    void introduce(Address addr);
    void add_task(TaskT f);
    void fulfill(const IVarRef& ivar, std::vector<Data> vals);
    void add_trigger(const IVarRef& ivar, TriggerT trigger);
    void dec_ref(const IVarRef& ivar);
    void recv();
    void one_step();
    void run();
    void set_core_affinity(int core_id);
};

extern thread_local Worker* cur_worker;

bool can_run_immediately();
int n_workers();

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
