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

IVarRef::IVarRef(int owner, size_t id, Scheduler* s):
    owner(owner), id(id), s(s)
{
    s->ivars[id].ref_count++;
}

IVarRef::~IVarRef() {
    auto& ref_count = s->ivars[id].ref_count;
    ref_count--;
    static int erase_count = 0;
    if (ref_count == 0) {
        s->ivars.erase(id);
        erase_count++;
        if (erase_count % 10000 == 0) {
            std::cout << erase_count << std::endl;
        }
    }
}

IVarRef::IVarRef(const IVarRef& ref):
    IVarRef(ref.owner, ref.id, ref.s)
{}

IVarRef& IVarRef::operator=(const IVarRef& ref) {
    owner = ref.owner; 
    id = ref.id; 
    s = ref.s;
    s->ivars[id].ref_count++;
    return *this;
}

void IVarRef::fulfill(std::vector<Data> vals) 
{
    assert(vals.size() > 0);

    auto& ivar_data = s->ivars[id];
    assert(ivar_data.vals.size() == 0);

    ivar_data.vals = std::move(vals);
    for (auto& t: ivar_data.fulfill_triggers) {
        t(s, ivar_data.vals);
    }
}

void IVarRef::add_trigger(TriggerT trigger)
{
    auto& ivar_data = s->ivars[id];
    if (ivar_data.vals.size() > 0) {
        trigger(s, ivar_data.vals);
    } else {
        ivar_data.fulfill_triggers.push_back(trigger);
    }
}

IVarRef run_then(const Then& then, Scheduler* s) {
    auto inside = run_helper(*then.child.get(), s);
    auto outside = s->new_ivar();
    inside.add_trigger(
        [outside, fnc = then.fnc] 
        (Scheduler* s, std::vector<Data>& vals) mutable {
            s->add_task(
                [outside, vals, fnc = std::move(fnc)]
                (Scheduler* s) mutable {
                    (void)s;
                    outside.fulfill({fnc(vals)});
                }
            );
        }
    );
    return outside;
}

IVarRef run_unwrap(const Unwrap& unwrap, Scheduler* s) {
    auto inside = run_helper(*unwrap.child.get(), s);
    auto fnc = unwrap.fnc;
    auto out_future = s->new_ivar();
    inside.add_trigger(
        [fnc = std::move(fnc), out_future]
        (Scheduler* s, std::vector<Data>& vals) mutable {
            auto out = fnc(vals);
            auto future_data = out.get_as<std::shared_ptr<FutureNode>>();
            auto result = run_helper(*future_data, s);
            result.add_trigger([out_future]
                (Scheduler* s, std::vector<Data>& vals) mutable {
                    (void)s;
                    out_future.fulfill(vals); 
                }
            );
        }
    );
    return out_future;
}

IVarRef run_async(const Async& async, Scheduler* s) {
    auto out_future = s->new_ivar();
    auto fnc = async.fnc;
    s->add_task(
        [out_future, fnc = std::move(fnc)]
        (Scheduler* s) mutable {
            (void)s;
            std::vector<Data> empty;
            out_future.fulfill({fnc(empty)});
        }
    );
    return out_future;
}

IVarRef run_ready(const Ready& ready, Scheduler* s) {
    auto out = s->new_ivar();
    out.fulfill({Data{ready.data}});
    return out;
}

void whenall_child(std::vector<std::shared_ptr<FutureNode>> children, Scheduler* s,
    std::vector<Data> accumulator, IVarRef result) 
{
    auto child_run = run_helper(*children.back().get(), s);
    children.pop_back();
    if (children.size() == 0) {
        child_run.add_trigger(
            [
                result = std::move(result),
                accumulator = std::move(accumulator)
            ]
            (Scheduler* s, std::vector<Data>& vals) mutable {
                (void)s;
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

IVarRef run_whenall(const WhenAll& whenall, Scheduler* s) {
    auto result = s->new_ivar();
    whenall_child(whenall.children, s, {}, result);
    return result;
}

IVarRef run_helper(const FutureNode& data, Scheduler* s) {
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
