#include "run.hpp"
#include "worker.hpp"

#include <unordered_map>
#include <vector>
#include <queue>
#include <thread>
#include <atomic>
#include <iostream>

namespace taskloaf {

typedef Function<Data(std::vector<Data>&)> PureTaskT;

void whenall_child(std::queue<IVarRef> child_results,
    std::vector<Data> accumulator, IVarRef result) 
{
    auto next_result = std::move(child_results.front());
    child_results.pop();
    if (child_results.size() == 0) {
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
                result = std::move(result),
                accumulator = std::move(accumulator)
            ]
            (std::vector<Data>& vals) mutable {
                accumulator.push_back(vals[0]);
                whenall_child(std::move(child_results), 
                    std::move(accumulator), std::move(result)
                );
            }
        );
    }
}

struct Planner {
    IVarRef plan(FutureNode& data) {
        auto result = cur_worker->new_ivar(data.id);
        if (!result.second) {
            return std::move(result.first);
        }

        if (data.type == ThenType) {
            plan_then(reinterpret_cast<Then&>(data), result.first);
        } else if (data.type == UnwrapType) {
            plan_unwrap(reinterpret_cast<Unwrap&>(data), result.first);
        } else if (data.type == AsyncType) {
            plan_async(reinterpret_cast<Async&>(data), result.first);
        } else if (data.type == ReadyType) {
            plan_ready(reinterpret_cast<Ready&>(data), result.first);
        } else if (data.type == WhenAllType) {
            plan_whenall(reinterpret_cast<WhenAll&>(data), result.first);
        }

        return std::move(result.first);
    }

    void plan_then(Then& then, const IVarRef& out_future) {
        auto inside = plan(*then.child.get());
        cur_worker->add_trigger(inside,
            [out_future = out_future, fnc = PureTaskT(std::move(then.fnc))] 
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
    }

    void plan_unwrap(Unwrap& unwrap, const IVarRef& out_future) {
        auto inside = plan(*unwrap.child.get());
        cur_worker->add_trigger(inside,
            [out_future = out_future, fnc = PureTaskT(std::move(unwrap.fnc))]
            (std::vector<Data>& vals) mutable {
                auto out = fnc(vals);
                auto future_data = out.get_as<std::shared_ptr<FutureNode>>();
                Planner planner;
                auto result = planner.plan(*future_data);
                cur_worker->add_trigger(result,
                    [out_future = std::move(out_future)]
                    (std::vector<Data>& vals) mutable {
                        cur_worker->fulfill(out_future, vals); 
                    }
                );
            }
        );
    }

    void plan_async(Async& async, const IVarRef& out_future) {
        cur_worker->add_task(
            [out_future = out_future, fnc = PureTaskT(std::move(async.fnc))]
            () mutable {
                std::vector<Data> empty;
                cur_worker->fulfill(out_future, {fnc(empty)});
            }
        );
    }

    void plan_ready(Ready& ready, const IVarRef& out_future) {
        cur_worker->fulfill(out_future, {Data{std::move(ready.data)}});
    }

    void plan_whenall(const WhenAll& whenall, const IVarRef& out_future) {
        std::queue<IVarRef> child_results;
        for (size_t i = 0; i < whenall.children.size(); i++) {
            child_results.push(plan(*whenall.children[i]));
        }
        whenall_child(std::move(child_results), {}, out_future);
    }
};

void launch_helper(size_t n_workers, std::shared_ptr<FutureNode> f) {
    std::vector<Address> addrs(n_workers);
    std::vector<std::thread> threads;
    std::atomic<size_t> n_spawned(0);
    std::atomic<size_t> n_ready(0);
    for (size_t i = 0; i < n_workers; i++) { 
        threads.emplace_back(
            [f, i, n_workers, &addrs, &n_spawned, &n_ready] () mutable {

                Worker w;
                w.set_core_affinity(i);
                cur_worker = &w;
                addrs[i] = w.get_addr();
                n_spawned++;
                while (n_workers > n_spawned) {}

                for (size_t j = 0; j < n_workers; j++) {
                    w.introduce(addrs[j]); 
                }
                while (w.ivar_tracker.ring_members().size() < n_workers) {
                    w.recv();
                }
                n_ready++;
                while (n_workers > n_ready) {}

                assert(w.ivar_tracker.ring_members().size() == n_workers);
                if (i == n_workers - 1) {
                    Planner planner;
                    planner.plan(*f);
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


void run_helper(FutureNode& data) {
    Worker w;
    cur_worker = &w;
    Planner planner;
    planner.plan(data);
    w.run();
}

} //end namespace taskloaf
