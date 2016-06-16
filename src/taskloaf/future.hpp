#pragma once
#include "location.hpp"
#include "ivar.hpp"

#include <cereal/types/tuple.hpp>

namespace taskloaf {

struct future {
    ivar internal;

    void save(cereal::BinaryOutputArchive& ar) const {
        (void)ar;
    }

    void load(cereal::BinaryInputArchive& ar) {
        (void)ar;
    }

    future then(closure fnc) {
        return then(location::anywhere, std::move(fnc));
    }

    future then(location loc, closure fnc) {
        return then(static_cast<int>(loc), std::move(fnc));
    }

    future then(int loc, closure fnc) {
        future out_future; 
        auto iloc = internal_loc(loc);
        internal.add_trigger(closure(
            [] (std::tuple<future,closure,internal_location>& d, data& v) {
                auto t = closure(
                    [] (std::tuple<future,closure,data>& d, ignore) {
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
            std::make_tuple(out_future, std::move(fnc), std::move(iloc))
        ));
        return out_future;
    }

    future unwrap() {
        future out_future;
        internal.add_trigger(closure(
            [] (future& out_future, future& f) {
                f.internal.add_trigger(closure(
                    [] (future& out_future, data& d) {
                        out_future.internal.fulfill(std::move(d));
                        return ignore{};
                    },
                    std::move(out_future)
                ));
                return ignore{};
            },
            out_future
        ));
        return out_future;
    }

    data get() {
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
        
    bool fulfilled_here() const {
        return internal.fulfilled_here();
    }
};

future ready(data d) {
    future out;
    out.internal.fulfill(std::move(d));
    return out;
}

template <typename T>
future ready(T&& v) { return ready(data(std::forward<T>(v))); }

future async(int loc, closure fnc) {
    future ofuture;
    auto iloc = internal_loc(loc);
    auto t = closure(
        [] (std::tuple<future,closure>& d, ignore) {
            std::get<0>(d).internal.fulfill(std::get<1>(d)());
            return ignore{};
        },
        std::make_tuple(ofuture, std::move(fnc))
    );
    schedule(iloc, std::move(t));
    return ofuture;
}

future async(location loc, closure fnc) {
    return async(static_cast<int>(loc), std::move(fnc));
}

future async(closure fnc) {
    return async(location::anywhere, std::move(fnc));
}

} //end namespace taskloaf
