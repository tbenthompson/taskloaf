#include "run.hpp"

namespace taskloaf {

IVarRef Worker::new_ivar() {
    auto id = next_ivar_id;
    next_ivar_id++;
    ivars.insert({id, {}});
    return IVarRef(0, id);
}

void Worker::add_task(TaskT f) 
{
    tasks.push(std::move(f));
}

void Worker::run()
{
    while (!tasks.empty()) {
        auto t = std::move(tasks.top());
        tasks.pop();
        t();
    }
}

IVarRef::IVarRef(int owner, size_t id):
    owner(owner), id(id)
{
    assert(cur_worker->ivars.count(id) > 0);
    inc_ref();
}

IVarRef::~IVarRef() {
    assert(cur_worker->ivars.count(id) > 0);
    dec_ref();
}

IVarRef::IVarRef(IVarRef&& ref):
    IVarRef(std::move(ref.owner), std::move(ref.id))
{}

IVarRef::IVarRef(const IVarRef& ref):
    IVarRef(ref.owner, ref.id)
{}

void IVarRef::inc_ref() {
    cur_worker->ivars[id].ref_count++;
}

void IVarRef::dec_ref() {
    auto& ref_count = cur_worker->ivars[id].ref_count;
    ref_count--;
    if (ref_count == 0) {
        cur_worker->ivars.erase(id);
    }
}


void IVarRef::fulfill(std::vector<Data> vals) {
    assert(vals.size() > 0);

    auto& ivar_data = cur_worker->ivars[id];
    assert(ivar_data.vals.size() == 0);

    ivar_data.vals = std::move(vals);
    for (auto& t: ivar_data.fulfill_triggers) {
        t(ivar_data.vals);
    }
}

void IVarRef::add_trigger(TriggerT trigger)
{
    auto& ivar_data = cur_worker->ivars[id];
    if (ivar_data.vals.size() > 0) {
        trigger(ivar_data.vals);
    } else {
        ivar_data.fulfill_triggers.push_back(std::move(trigger));
    }
}

IVarRef run_then(const Then& then) {
    auto inside = run_helper(*then.child.get());
    auto outside = cur_worker->new_ivar();
    inside.add_trigger(
        [outside, fnc = PureTaskT(then.fnc)] 
        (std::vector<Data>& vals) mutable {
            cur_worker->add_task(
                [outside, vals, fnc = std::move(fnc)]
                () mutable {
                    outside.fulfill({fnc(vals)});
                }
            );
        }
    );
    return outside;
}

IVarRef run_unwrap(const Unwrap& unwrap) {
    auto inside = run_helper(*unwrap.child.get());
    auto out_future = cur_worker->new_ivar();
    inside.add_trigger(
        [out_future, fnc = PureTaskT(unwrap.fnc)]
        (std::vector<Data>& vals) mutable {
            auto out = fnc(vals);
            auto future_data = out.get_as<std::shared_ptr<FutureNode>>();
            auto result = run_helper(*future_data);
            result.add_trigger([out_future]
                (std::vector<Data>& vals) mutable {
                    out_future.fulfill(vals); 
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
            out_future.fulfill({fnc(empty)});
        }
    );
    return out_future;
}

IVarRef run_ready(const Ready& ready) {
    auto out = cur_worker->new_ivar();
    out.fulfill({Data{ready.data}});
    return out;
}

void whenall_child(std::vector<std::shared_ptr<FutureNode>> children,
    std::vector<Data> accumulator, IVarRef result) 
{
    auto child_run = run_helper(*children.back().get());
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
