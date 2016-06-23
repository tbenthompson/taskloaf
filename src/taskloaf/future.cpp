#include "future.hpp"
#include "worker.hpp"


namespace taskloaf {

void untyped_future::save(cereal::BinaryOutputArchive& ar) const {
    ar(internal);
}

void untyped_future::load(cereal::BinaryInputArchive& ar) {
    ar(internal);
}

untyped_future untyped_future::then(closure fnc) {
    return then(location::anywhere, std::move(fnc));
}

untyped_future untyped_future::then(location loc, closure fnc) {
    return then(static_cast<int>(loc), std::move(fnc));
}

untyped_future untyped_future::then(int loc, closure fnc) {
    untyped_future out_untyped_future; 
    auto iloc = internal_loc(loc);
    internal.add_trigger(closure(
        [] (std::tuple<untyped_future,closure,internal_location>& d, data& v) {
            auto t = closure(
                [] (std::tuple<untyped_future,closure,data>& d, ignore) {
                    std::get<0>(d).internal.fulfill(std::get<1>(d)(std::get<2>(d)));
                    return ignore{};
                },
                std::make_tuple(
                    std::move(std::get<0>(d)), std::move(std::get<1>(d)), v
                )
            );
            schedule(std::get<2>(d), std::move(t));
            return ignore{};
        },
        std::make_tuple(out_untyped_future, std::move(fnc), std::move(iloc))
    ));
    return out_untyped_future;
}

untyped_future untyped_future::unwrap() {
    untyped_future out_untyped_future;
    internal.add_trigger(closure(
        [] (untyped_future& out_untyped_future, untyped_future& f) {
            f.internal.add_trigger(closure(
                [] (untyped_future& out_untyped_future, data& d) {
                    out_untyped_future.internal.fulfill(std::move(d));
                    return ignore{};
                },
                std::move(out_untyped_future)
            ));
            return ignore{};
        },
        out_untyped_future
    ));
    return out_untyped_future;
}

data untyped_future::get() {
    if (fulfilled_here()) {
        return internal.get();
    }

    data out;
    cur_worker->set_stopped(false);
    this->then(location::here, closure(
        [&] (ignore, data& val) {
            cur_worker->set_stopped(true);
            out = val;
            return ignore{};
        }
    ));
    cur_worker->run();
    return out;
}
    
bool untyped_future::fulfilled_here() const {
    return internal.fulfilled_here();
}

untyped_future ut_ready(data d) {
    untyped_future out;
    out.internal.fulfill(std::move(d));
    return out;
}

untyped_future ut_async(int loc, closure fnc) {
    untyped_future ountyped_future;
    auto iloc = internal_loc(loc);
    auto t = closure(
        [] (std::tuple<untyped_future,closure>& d, ignore) {
            std::get<0>(d).internal.fulfill(std::get<1>(d)());
            return ignore{};
        },
        std::make_tuple(ountyped_future, std::move(fnc))
    );
    schedule(iloc, std::move(t));
    return ountyped_future;
}

untyped_future ut_async(location loc, closure fnc) {
    return ut_async(static_cast<int>(loc), std::move(fnc));
}

untyped_future ut_async(closure fnc) {
    return ut_async(location::anywhere, std::move(fnc));
}

}
