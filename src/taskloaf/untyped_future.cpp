#include "untyped_future.hpp"

namespace taskloaf {

IVar::IVar(): data(std::make_shared<IVarData>()) {
    data->owner = cur_worker->get_addr();
}

void IVar::add_trigger(TriggerT trigger) {
    auto action = [] (std::shared_ptr<IVarData>& data, 
        TriggerT& trigger) 
    {
        if (data->fulfilled) {
            trigger(data->vals);
        } else {
            data->triggers.push_back(std::move(trigger));
        }
    };
    if (data->owner == cur_worker->get_addr()) {
        action(data, trigger); 
    } else {
        cur_worker->add_task(data->owner, TaskT(
            action, data, std::move(trigger)
        ));
    }
}

void IVar::fulfill(std::vector<Data> vals) {
    auto action = [] (std::shared_ptr<IVarData>& data, std::vector<Data>& vals) {
        tlassert(!data->fulfilled);
        data->fulfilled = true;
        data->vals = std::move(vals);
        for (auto& t: data->triggers) {
            t(data->vals);
        }
        data->triggers.clear();
    };
    if (data->owner == cur_worker->get_addr()) {
        action(data, vals); 
    } else {
        cur_worker->add_task(data->owner, TaskT(
            action, data, std::move(vals)
        ));
    }
}

std::vector<Data> IVar::get_vals() {
    return data->vals;
}

void UntypedFuture::add_trigger(TriggerT trigger) {
    ivar.add_trigger(std::move(trigger));
}

void UntypedFuture::fulfill(std::vector<Data> vals) {
    ivar.fulfill(std::move(vals));
}

void UntypedFuture::save(cereal::BinaryOutputArchive& ar) const {
    (void)ar;
}

void UntypedFuture::load(cereal::BinaryInputArchive& ar) {
    (void)ar;
}

UntypedFuture UntypedFuture::then(ThenTaskT f) {
    return then(Loc::anywhere, std::move(f));
}

UntypedFuture UntypedFuture::then(Loc loc, ThenTaskT f) {
    return then(static_cast<int>(loc), std::move(f));
}

UntypedFuture UntypedFuture::then(int loc, ThenTaskT f) {
    return taskloaf::then(loc, *this, std::move(f));
}

UntypedFuture UntypedFuture::unwrap() {
    return taskloaf::unwrap(*this); 
}

std::vector<Data> UntypedFuture::get() {
    wait();
    return ivar.get_vals();
}

void UntypedFuture::wait() {
    cur_worker->set_stopped(false);
    bool already_thenned = false;
    this->then(Loc::here, ThenTaskT(
        [&] (std::vector<Data>&) {
            cur_worker->set_stopped(true);
            already_thenned = true;
            return std::vector<Data>{};
        }
    ));
    if (!already_thenned) {
        cur_worker->run();
    }
}

UntypedFuture ready(std::vector<Data> d) {
    UntypedFuture out;
    out.fulfill({d});
    return out;
}

UntypedFuture then(int loc, UntypedFuture& f, ThenTaskT fnc) {
    UntypedFuture out_future; 
    auto iloc = internal_loc(loc);
    f.add_trigger(TriggerT(
        [] (UntypedFuture& out_future, ThenTaskT& fnc,
            InternalLoc& iloc, std::vector<Data>& d) 
        {
            TaskT t(
                [] (UntypedFuture& out_future, ThenTaskT& fnc, std::vector<Data>& d) {
                    out_future.fulfill(fnc(d));
                },
                std::move(out_future), std::move(fnc), d
            );
            schedule(iloc, std::move(t));
        },
        out_future, std::move(fnc), std::move(iloc)
    ));
    return out_future;
}

UntypedFuture async(int loc, AsyncTaskT fnc) {
    UntypedFuture out_future;
    auto iloc = internal_loc(loc);
    TaskT t(
        [] (UntypedFuture& out_future, AsyncTaskT& fnc) {
            out_future.fulfill(fnc());
        },
        out_future, std::move(fnc)
    );
    schedule(iloc, std::move(t));
    return out_future;
}

UntypedFuture async(Loc loc, AsyncTaskT fnc) {
    return async(static_cast<int>(loc), std::move(fnc));
}

UntypedFuture async(AsyncTaskT fnc) {
    return async(Loc::anywhere, std::move(fnc));
}

UntypedFuture unwrap(UntypedFuture& fut) {
    UntypedFuture out_future;
    fut.add_trigger(TriggerT(
        [] (UntypedFuture& out_future, std::vector<Data>& d) {
            tlassert(d.size() == 1);
            d[0].get_as<UntypedFuture>().add_trigger(TriggerT(
                [] (UntypedFuture& out_future, std::vector<Data>& d) {
                    out_future.fulfill(d);
                },
                std::move(out_future)
            ));
        },
        out_future
    ));
    return out_future;
}

}
