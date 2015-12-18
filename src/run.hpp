#pragma once
#include "future.hpp"

#include <stack>
#include <iostream>

namespace taskloaf {

struct Scheduler {
    std::stack<std::function<void()>> tasks;

    template <typename F>
    void add_task(F f) 
    {
        tasks.push(f);
    }

    void run()
    {
        while (!tasks.empty()) {
            auto t = std::move(tasks.top());
            tasks.pop();
            t();
        }
    }
};

struct STFData {
    std::unique_ptr<Data> val;
    std::vector<std::function<void(Data& val)>> fulfill_triggers;
};

struct STF {
    std::shared_ptr<STFData> data;

    STF(): data(std::make_shared<STFData>()) {}

    void fulfill(Data val) 
    {
        assert(data->val == nullptr);
        data->val.reset(new Data(std::move(val)));
        for (auto& t: data->fulfill_triggers) {
            t(*data->val);
        }
    }

    void add_trigger(std::function<void(Data& val)> trigger)
    {
        if (data->val != nullptr) {
            trigger(*data->val);
        } else {
            data->fulfill_triggers.push_back(trigger);
        }
    }
};

inline STF run_helper(const FutureData& data, Scheduler* s) {
    if (const Then* then = dynamic_cast<const Then*>(&data)) {
        auto inside = run_helper(*then->child.get(), s);
        STF outside;
        inside.add_trigger([=] (Data& val) mutable {
            s->add_task([=] () mutable {
                Data out;
                std::vector<Data*> input{&val, &out};
                fnc_registry[then->fnc_name](input);
                outside.fulfill(std::move(out));
            });
        });
        return outside;
    } else if (const Unwrap* unwrap = dynamic_cast<const Unwrap*>(&data)) {
    } else if (const Async* async = dynamic_cast<const Async*>(&data)) {
        STF out_future;
        s->add_task([=] () mutable {
            Data out;
            std::vector<Data*> input{&out};
            fnc_registry[async->fnc_name](input);
            out_future.fulfill(std::move(out));
        });
        return out_future;
    } else if (const Ready* ready = dynamic_cast<const Ready*>(&data)) {
        STF out;
        out.fulfill(Data{ready->data});
        return out;
    } else if (const WhenAll* whenall = dynamic_cast<const WhenAll*>(&data)) {
        STF result;
        for (auto& c: whenall->children) {
            auto child_run = run_helper(*c.get(), s);
            child_run.add_trigger([=] (Data& val) mutable {
                //something here
            });
        }
        return result;
    }
    return STF();
}

template <typename T>
void run(const Future<T>& fut, Scheduler& s) {
    run_helper(*fut.data.get(), &s);
    s.run();
}

}
