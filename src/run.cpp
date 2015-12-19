#include "run.hpp"

namespace taskloaf {

void Scheduler::run()
{
    while (!tasks.empty()) {
        auto t = std::move(tasks.top());
        tasks.pop();
        t();
    }
}

STF::STF(): 
    data(std::make_shared<STFData>()) 
{}

void STF::fulfill(std::vector<Data> vals) 
{
    assert(data->vals.size() == 0);
    assert(vals.size() > 0);
    data->vals = std::move(vals);
    for (auto& t: data->fulfill_triggers) {
        t(data->vals);
    }
}

void STF::add_trigger(Function<void(std::vector<Data>& val)> trigger)
{
    if (data->vals.size() > 0) {
        trigger(data->vals);
    } else {
        data->fulfill_triggers.push_back(trigger);
    }
}

STF run_then(const Then& then, Scheduler* s) {
    auto inside = run_helper(*then.child.get(), s);
    auto fnc = then.fnc;
    STF outside;
    inside.add_trigger(
        [s, outside, fnc = std::move(fnc)] 
        (std::vector<Data>& vals) mutable {
            s->add_task(
                [outside, vals, fnc = std::move(fnc)]
                () mutable {
                    outside.fulfill({fnc(vals)});
                }
            );
        }
    );
    return outside;
}

STF run_unwrap(const Unwrap& unwrap, Scheduler* s) {
    auto inside = run_helper(*unwrap.child.get(), s);
    auto fnc = unwrap.fnc;
    STF out_future;
    inside.add_trigger([=] (std::vector<Data>& vals) mutable {
        auto out = fnc(vals);
        auto future_data = out.get_as<std::shared_ptr<FutureNode>>();
        auto result = run_helper(*future_data, s);
        result.add_trigger([=] (std::vector<Data>& vals) mutable {
            out_future.fulfill(vals); 
        });
    });
    return out_future;
}

STF run_async(const Async& async, Scheduler* s) {
    STF out_future;
    auto fnc = async.fnc;
    s->add_task([=] () mutable {
        std::vector<Data> empty;
        out_future.fulfill({fnc(empty)});
    });
    return out_future;
}

STF run_ready(const Ready& ready, Scheduler* s) {
    (void)s;
    STF out;
    out.fulfill({Data{ready.data}});
    return out;
}

void whenall_child(std::vector<std::shared_ptr<FutureNode>> children, Scheduler* s,
    std::vector<Data> accumulator, STF result) 
{
    auto child_run = run_helper(*children.back().get(), s);
    children.pop_back();
    if (children.size() == 0) {
        child_run.add_trigger(
            [
                result = std::move(result),
                accumulator = std::move(accumulator)
            ]
            (std::vector<Data>& vals) mutable {
                accumulator.push_back(vals[0]);
                result.fulfill(std::move(accumulator));
            }
        );
    } else {
        child_run.add_trigger(
            [
                children = std::move(children),
                result = std::move(result),
                accumulator = std::move(accumulator),
                s
            ]
            (std::vector<Data>& vals) mutable {
                accumulator.push_back(vals[0]);
                whenall_child(
                    std::move(children), s, 
                    std::move(accumulator), std::move(result)
                );
            }
        );
    }
}

STF run_whenall(const WhenAll& whenall, Scheduler* s) {
    STF result;
    whenall_child(whenall.children, s, {}, result);
    return result;
}

STF run_helper(const FutureNode& data, Scheduler* s) {
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
