#include "run.hpp"

namespace taskloaf {

typedef Function<Data(std::vector<Data>&)> PureTaskT;

IVarRef run_then(const Then& then) {
    auto inside = run_helper(*then.child.get());
    auto outside = cur_worker->new_ivar();
    cur_worker->add_trigger(inside,
        [outside, fnc = PureTaskT(then.fnc)] 
        (std::vector<Data>& vals) mutable {
            cur_worker->add_task(
                [
                    outside = std::move(outside),
                    vals, fnc = std::move(fnc)
                ]
                () mutable {
                    cur_worker->fulfill(outside, {fnc(vals)});
                }
            );
        }
    );
    return outside;
}

IVarRef run_unwrap(const Unwrap& unwrap) {
    auto inside = run_helper(*unwrap.child.get());
    auto out_future = cur_worker->new_ivar();
    cur_worker->add_trigger(inside,
        [out_future, fnc = PureTaskT(unwrap.fnc)]
        (std::vector<Data>& vals) mutable {
            auto out = fnc(vals);
            auto future_data = out.get_as<std::shared_ptr<FutureNode>>();
            auto result = run_helper(*future_data);
            cur_worker->add_trigger(result,
                [out_future] (std::vector<Data>& vals) mutable {
                    cur_worker->fulfill(out_future, vals); 
                }
            );
        }
    );
    return out_future;
}

IVarRef run_async(const Async& async) {
    auto out_future = cur_worker->new_ivar();
    cur_worker->add_task(
        [out_future, fnc = PureTaskT(async.fnc)]
        () mutable {
            std::vector<Data> empty;
            cur_worker->fulfill(out_future, {fnc(empty)});
        }
    );
    return out_future;
}

IVarRef run_ready(const Ready& ready) {
    auto out = cur_worker->new_ivar();
    cur_worker->fulfill(out, {Data{ready.data}});
    return out;
}

void whenall_child(std::vector<std::shared_ptr<FutureNode>> children,
    std::vector<Data> accumulator, IVarRef result) 
{
    auto child_run = run_helper(*children.back().get());
    children.pop_back();
    if (children.size() == 0) {
        cur_worker->add_trigger(child_run,
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
        cur_worker->add_trigger(child_run,
            [
                children = std::move(children),
                result = std::move(result),
                accumulator = std::move(accumulator)
            ]
            (std::vector<Data>& vals) mutable {
                accumulator.push_back(vals[0]);
                whenall_child(std::move(children), 
                    std::move(accumulator), std::move(result)
                );
            }
        );
    }
}

IVarRef run_whenall(const WhenAll& whenall) {
    auto result = cur_worker->new_ivar();
    whenall_child(whenall.children, {}, result);
    return result;
}

IVarRef run_helper(const FutureNode& data) {
    switch (data.type) {
        case ThenType:
            return run_then(reinterpret_cast<const Then&>(data));

        case UnwrapType:
            return run_unwrap(reinterpret_cast<const Unwrap&>(data));

        case AsyncType:
            return run_async(reinterpret_cast<const Async&>(data));

        case ReadyType:
            return run_ready(reinterpret_cast<const Ready&>(data));

        case WhenAllType:
            return run_whenall(reinterpret_cast<const WhenAll&>(data));

        default: 
            throw std::runtime_error("Unknown FutureNode type");
    };
}


} //end namespace taskloaf
