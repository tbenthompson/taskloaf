#include "run.hpp"

namespace taskloaf {

void Scheduler::run()
{
    while (!tasks.empty()) {
        auto t = std::move(tasks.top());
        tasks.pop();
        t(this);
    }
}

IVar::IVar(): 
    data(std::make_shared<IVarData>()) 
{}

void IVar::fulfill(Scheduler* s, std::vector<Data> vals) 
{
    assert(data->vals.size() == 0);
    assert(vals.size() > 0);
    data->vals = std::move(vals);
    for (auto& t: data->fulfill_triggers) {
        t(s, data->vals);
    }
}

void IVar::add_trigger(Scheduler* s, TriggerT trigger)
{
    if (data->vals.size() > 0) {
        trigger(s, data->vals);
    } else {
        data->fulfill_triggers.push_back(trigger);
    }
}

IVar run_then(const Then& then, Scheduler* s) {
    auto inside = run_helper(*then.child.get(), s);
    auto fnc = then.fnc;
    IVar outside;
    inside.add_trigger(s,
        [outside, fnc = std::move(fnc)] 
        (Scheduler* s, std::vector<Data>& vals) mutable {
            s->add_task(
                [outside, vals, fnc = std::move(fnc)]
                (Scheduler* s) mutable {
                    outside.fulfill(s, {fnc(vals)});
                }
            );
        }
    );
    return outside;
}

IVar run_unwrap(const Unwrap& unwrap, Scheduler* s) {
    auto inside = run_helper(*unwrap.child.get(), s);
    auto fnc = unwrap.fnc;
    IVar out_future;
    inside.add_trigger(s, [fnc, out_future]
        (Scheduler* s, std::vector<Data>& vals) mutable {
            auto out = fnc(vals);
            auto future_data = out.get_as<std::shared_ptr<FutureNode>>();
            auto result = run_helper(*future_data, s);
            result.add_trigger(s, [out_future]
                (Scheduler* s, std::vector<Data>& vals) mutable {
                    out_future.fulfill(s, vals); 
                }
            );
        }
    );
    return out_future;
}

IVar run_async(const Async& async, Scheduler* s) {
    IVar out_future;
    auto fnc = async.fnc;
    s->add_task([out_future, fnc] (Scheduler* s) mutable {
        std::vector<Data> empty;
        out_future.fulfill(s, {fnc(empty)});
    });
    return out_future;
}

IVar run_ready(const Ready& ready, Scheduler* s) {
    IVar out;
    out.fulfill(s, {Data{ready.data}});
    return out;
}

void whenall_child(std::vector<std::shared_ptr<FutureNode>> children, Scheduler* s,
    std::vector<Data> accumulator, IVar result) 
{
    auto child_run = run_helper(*children.back().get(), s);
    children.pop_back();
    if (children.size() == 0) {
        child_run.add_trigger(s,
            [
                result = std::move(result),
                accumulator = std::move(accumulator)
            ]
            (Scheduler* s, std::vector<Data>& vals) mutable {
                accumulator.push_back(vals[0]);
                result.fulfill(s, std::move(accumulator));
            }
        );
    } else {
        child_run.add_trigger(s,
            [
                children = std::move(children),
                result = std::move(result),
                accumulator = std::move(accumulator)
            ]
            (Scheduler* s, std::vector<Data>& vals) mutable {
                accumulator.push_back(vals[0]);
                whenall_child(
                    std::move(children), s, 
                    std::move(accumulator), std::move(result)
                );
            }
        );
    }
}

IVar run_whenall(const WhenAll& whenall, Scheduler* s) {
    IVar result;
    whenall_child(whenall.children, s, {}, result);
    return result;
}

IVar run_helper(const FutureNode& data, Scheduler* s) {
    switch (data.type) {
        case ThenType:
            return run_then(reinterpret_cast<const Then&>(data), s);

        case UnwrapType:
            return run_unwrap(reinterpret_cast<const Unwrap&>(data), s);

        case AsyncType:
            return run_async(reinterpret_cast<const Async&>(data), s);

        case ReadyType:
            return run_ready(reinterpret_cast<const Ready&>(data), s);

        case WhenAllType:
            return run_whenall(reinterpret_cast<const WhenAll&>(data), s);

        default: 
            throw std::runtime_error("Unknown FutureNode type");
    };
}


} //end namespace taskloaf
