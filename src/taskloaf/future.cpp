#include "future.hpp"
#include "worker.hpp"

#include <queue>
#include <thread>
#include <atomic>

namespace taskloaf {

IVarRef plan_then(const IVarRef& input, PureTaskT task) {
    IVarRef out_future(new_id());
    cur_worker->add_trigger(input,
        [out_future = out_future, fnc = std::move(task)] 
        (std::vector<Data>& vals) mutable {
            cur_worker->add_task(
                [
                    out_future = std::move(out_future),
                    vals = std::vector<Data>(vals),
                    fnc = std::move(fnc)
                ]
                () mutable {
                    cur_worker->fulfill(out_future, {fnc(vals)});
                }
            );
        }
    );
    return std::move(out_future);
}

IVarRef plan_unwrap(const IVarRef& input, PureTaskT unwrapper) {
    IVarRef out_future(new_id());
    cur_worker->add_trigger(input,
        [out_future = out_future, fnc = std::move(unwrapper)]
        (std::vector<Data>& vals) mutable {
            auto result = fnc(vals).get_as<IVarRef>();
            cur_worker->add_trigger(result,
                [out_future = std::move(out_future)]
                (std::vector<Data>& vals) mutable {
                    cur_worker->fulfill(out_future, vals); 
                }
            );
        }
    );
    return std::move(out_future);
}

IVarRef plan_ready(Data data) {
    IVarRef out_future(new_id());
    cur_worker->fulfill(out_future, {std::move(data)});
    return std::move(out_future);
}

IVarRef plan_async(PureTaskT task) {
    IVarRef out_future(new_id());
    cur_worker->add_task(
        [out_future = out_future, fnc = std::move(task)]
        () mutable {
            std::vector<Data> empty;
            cur_worker->fulfill(out_future, {fnc(empty)});
        }
    );
    return std::move(out_future);
}

void whenall_child(std::vector<IVarRef> child_results, size_t next_idx,
    std::vector<Data> accumulator, IVarRef result) 
{
    auto next_result = std::move(child_results[next_idx]);
    if (next_idx == child_results.size() - 1) {
        cur_worker->add_trigger(next_result,
            [
                result = std::move(result),
                accumulator = std::move(accumulator)
            ]
            (std::vector<Data>& vals) mutable {
                accumulator.push_back(vals[0]);
                cur_worker->fulfill(result, std::move(accumulator));
            }
        );
    } else {
        cur_worker->add_trigger(next_result,
            [
                child_results = std::move(child_results),
                next_idx,
                accumulator = std::move(accumulator),
                result = std::move(result)
            ]
            (std::vector<Data>& vals) mutable {
                accumulator.push_back(vals[0]);
                whenall_child(std::move(child_results), next_idx + 1,
                    std::move(accumulator), std::move(result)
                );
            }
        );
    }
}

IVarRef plan_when_all(std::vector<IVarRef> inputs) {
    IVarRef out_future(new_id());
    whenall_child(std::move(inputs), 0, {}, out_future);
    return std::move(out_future);
}

void launch_helper(size_t n_workers, std::function<IVarRef()> f) {
    std::vector<std::thread> threads;
    Address root_addr;
    std::atomic<bool> ready(false);
    for (size_t i = 0; i < n_workers; i++) { 
        threads.emplace_back(
            [f, i, &root_addr, &ready] () mutable {
                Worker w;
                cur_worker = &w;
                w.set_core_affinity(i);
                if (i == 0) {
                    root_addr = w.get_addr();
                    ready = true;
                    f();
                } else {
                    while (!ready) {}
                    w.introduce(root_addr); 
                }
                w.run();
            }
        );
    }

    for (auto& t: threads) { 
        t.join();         
    }
}

int shutdown() {
    cur_worker->shutdown(); 
    return 0;
}

} //end namespace taskloaf
