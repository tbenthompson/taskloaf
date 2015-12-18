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

void STF::add_trigger(std::function<void(std::vector<Data>& val)> trigger)
{
    if (data->vals.size() > 0) {
        trigger(data->vals);
    } else {
        data->fulfill_triggers.push_back(trigger);
    }
}

STF run_then(const Then* then, Scheduler* s) {
    auto inside = run_helper(*then->child.get(), s);
    STF outside;
    inside.add_trigger([=] (std::vector<Data>& vals) mutable {
        s->add_task([=] () mutable {
            Data out;
            std::vector<Data*> input;
            for (auto& v: vals) {
                input.push_back(&v);
            }
            input.push_back(&out);
            fnc_registry[then->fnc_name](input);
            outside.fulfill({std::move(out)});
        });
    });
    return outside;
}

STF run_unwrap(const Unwrap* unwrap, Scheduler* s) {
    auto inside = run_helper(*unwrap->child.get(), s);
    STF out_future;
    inside.add_trigger([=] (std::vector<Data>& vals) mutable {
        Data out;
        std::vector<Data*> fnc_input;
        fnc_input.push_back(&vals[0]);
        fnc_input.push_back(&out);
        fnc_registry[unwrap->fnc_name](fnc_input);
        auto future_data = out.get_as<std::shared_ptr<FutureData>>();
        auto result = run_helper(*future_data, s);
        result.add_trigger([=] (std::vector<Data>& vals) mutable {
            out_future.fulfill(vals); 
        });
    });
    return out_future;
}

STF run_async(const Async* async, Scheduler* s) {
    STF out_future;
    s->add_task([=] () mutable {
        Data out;
        std::vector<Data*> input{&out};
        fnc_registry[async->fnc_name](input);
        out_future.fulfill({std::move(out)});
    });
    return out_future;
}

STF run_ready(const Ready* ready, Scheduler* s) {
    (void)s;
    STF out;
    out.fulfill({Data{ready->data}});
    return out;
}

STF run_whenall(const WhenAll* whenall, Scheduler* s) {
    STF result;
    auto data_accum = std::make_shared<std::vector<Data>>();
    for (auto& c: whenall->children) {
        auto child_run = run_helper(*c.get(), s);
        child_run.add_trigger([=] (std::vector<Data>& vals) mutable {
            for (auto& v: vals) {
                data_accum->push_back(v);
            }
            if (data_accum->size() == whenall->children.size()) {
                result.fulfill(std::move(*data_accum)); 
            }
        });
    }
    return result;
}

STF run_helper(const FutureData& data, Scheduler* s) {
    switch (data.type) {
        case ThenType:
            return run_then(reinterpret_cast<const Then*>(&data), s);

        case UnwrapType:
            return run_unwrap(reinterpret_cast<const Unwrap*>(&data), s);

        case AsyncType:
            return run_async(reinterpret_cast<const Async*>(&data), s);

        case ReadyType:
            return run_ready(reinterpret_cast<const Ready*>(&data), s);

        case WhenAllType:
            return run_whenall(reinterpret_cast<const WhenAll*>(&data), s);

        default: 
            throw std::runtime_error("Unknown FutureData type");
    };
}


} //end namespace taskloaf
