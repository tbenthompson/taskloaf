#include "future.hpp"
#include "worker.hpp"

#include <cereal/types/tuple.hpp>

namespace taskloaf {

void future::save(cereal::BinaryOutputArchive& ar) const {
    (void)ar;
}

void future::load(cereal::BinaryInputArchive& ar) {
    (void)ar;
}

future future::then(closure f) {
    return then(location::anywhere, std::move(f));
}

future future::then(location loc, closure f) {
    return then(static_cast<int>(loc), std::move(f));
}

future future::then(int loc, closure f) {
    return taskloaf::then(loc, *this, std::move(f));
}

future future::unwrap() {
    return taskloaf::unwrap(*this); 
}

data& future::get() {
    if (!fulfilled()) {
        wait();
    }
    return internal.get();
}

void future::wait() {
    cur_worker->set_stopped(false);
    bool already_thenned = false;
    this->then(location::here, closure(
        [&] (ignore, ignore) {
            cur_worker->set_stopped(true);
            already_thenned = true;
            return ignore{};
        }
    ));
    if (!already_thenned) {
        cur_worker->run();
    }
}
    
bool future::fulfilled() const {
    return internal.fulfilled();
}

future ready(data d) {
    future out;
    out.internal.fulfill(std::move(d));
    return out;
}

future then(int loc, future& fut, closure fnc) {
    future out_future; 
    auto iloc = internal_loc(loc);
    fut.internal.add_trigger(closure(
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

future unwrap(future& fut) {
    future ofuture;
    fut.internal.add_trigger(closure(
        [] (future& ofuture, future& f) {
            f.internal.add_trigger(closure(
                [] (future& ofuture, data& d) {
                    ofuture.internal.fulfill(std::move(d));
                    return ignore{};
                },
                std::move(ofuture)
            ));
            return ignore{};
        },
        ofuture
    ));
    return ofuture;
}

// future when_both_leaf(future a, future b) {
//     return a.then(closure(
//         [] (future& b, Data& a_data) {
//             std::vector<Data> out{a_data};
//             out.push_back(make_data(b.then(closure(
//                 [] (std::vector<Data> a_data, std::vector<Data>& b_data) 
//                 {
//                     for (auto& b: b_data) {o
//                         a_data.push_back(b);
//                     }
//                     return a_data;
//                 },
//                 a_data
//             ))));
//             return out;
//         },
//         std::move(b)
//     )).unwrap();
// }
// 
// future when_all(std::vector<future> fs) {
//     TLASSERT(fs.size() != 0);
//     if (fs.size() == 1) {
//         return fs[0];
//     }
//     std::vector<future> recurse_fs;
//     for (size_t i = 0; i < fs.size(); i += 2) {
//         recurse_fs.push_back(when_both(fs[i], fs[i + 1]));
//     }
//     return when_all(recurse_fs);
// }

}
