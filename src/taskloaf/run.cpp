#include "run.hpp"
#include "worker.hpp"

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
    std::unordered_map<uintptr_t,IVarRef> done_nodes;

    std::pair<bool,IVarRef> new_ivar(const void* void_node_ptr) {
        auto node_ptr = reinterpret_cast<uintptr_t>(void_node_ptr);
        if (done_nodes.count(node_ptr) > 0) {
            return {false, done_nodes.at(node_ptr)};
        }
        auto ivref = cur_worker->new_ivar();
        done_nodes.insert({node_ptr, ivref});
        return {true, ivref};
    }

    IVarRef plan(const FutureNode& data) {
        auto result = new_ivar(&data);
        if (!result.first) {
            return result.second;
        }

        if (data.type == ThenType) {
            plan_then(reinterpret_cast<const Then&>(data), result.second);
        } else if (data.type == UnwrapType) {
            plan_unwrap(reinterpret_cast<const Unwrap&>(data), result.second);
        } else if (data.type == AsyncType) {
            plan_async(reinterpret_cast<const Async&>(data), result.second);
        } else if (data.type == ReadyType) {
            plan_ready(reinterpret_cast<const Ready&>(data), result.second);
        } else if (data.type == WhenAllType) {
            plan_whenall(reinterpret_cast<const WhenAll&>(data), result.second);
        }

        return result.second;
    }

    void plan_then(const Then& then, const IVarRef& out_future) {
        auto inside = plan(*then.child.get());
        cur_worker->add_trigger(inside,
            [out_future, fnc = PureTaskT(then.fnc)] 
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

    void plan_unwrap(const Unwrap& unwrap, const IVarRef& out_future) {
        auto inside = plan(*unwrap.child.get());
        cur_worker->add_trigger(inside,
            [out_future, fnc = PureTaskT(unwrap.fnc)]
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

    void plan_async(const Async& async, const IVarRef& out_future) {
        cur_worker->add_task(
            [out_future, fnc = PureTaskT(async.fnc)]
            () mutable {
                std::vector<Data> empty;
                cur_worker->fulfill(out_future, {fnc(empty)});
            }
        );
    }

    void plan_ready(const Ready& ready, const IVarRef& out_future) {
        cur_worker->fulfill(out_future, {Data{ready.data}});
    }

    void plan_whenall(const WhenAll& whenall, const IVarRef& out_future) {
        std::queue<IVarRef> child_results;
        for (size_t i = 0; i < whenall.children.size(); i++) {
            auto c = whenall.children[i];
            child_results.push(plan(*c));
        }
        whenall_child(std::move(child_results), {}, out_future);
    }
};

void launch_helper(int n_workers, std::shared_ptr<FutureNode> f) {
    std::vector<Address> addrs(n_workers);
    std::vector<std::thread> threads;
    for (int i = 0; i < n_workers; i++) { 
        std::atomic<bool> spawn_next(false);
        threads.emplace_back(
            [f, i, n_workers, &addrs, &spawn_next] () mutable {
                Worker w;
                cur_worker = &w;
                addrs[i] = w.get_addr();
                spawn_next = true;
                for (int j = 0; j < i; j++) {
                    w.meet(addrs[j]); 
                }
                if (i == n_workers - 1) {
                    Planner planner;
                    planner.plan(*f);
                }
                w.set_core_affinity(i);
                w.run();
            }
        );
        while(!spawn_next) { }
    }

    for (auto& t: threads) { 
        t.join();         
    }
}

int shutdown() {
    cur_worker->shutdown(); 
    return 0;
}


void run_helper(const FutureNode& data) {
    Worker w;
    cur_worker = &w;
    Planner planner;
    planner.plan(data);
    w.run();
}

} //end namespace taskloaf
