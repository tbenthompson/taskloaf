#include "untyped_future.hpp"
#include "worker.hpp"

#include <cereal/types/tuple.hpp>

namespace taskloaf {

void untyped_future::save(cereal::BinaryOutputArchive& ar) const {
    (void)ar;
}

void untyped_future::load(cereal::BinaryInputArchive& ar) {
    (void)ar;
}

untyped_future untyped_future::then(closure f) {
    return then(location::anywhere, std::move(f));
}

untyped_future untyped_future::then(location loc, closure f) {
    return then(static_cast<int>(loc), std::move(f));
}

untyped_future untyped_future::then(int loc, closure f) {
    return taskloaf::then(loc, *this, std::move(f));
}

// untyped_future untyped_future::unwrap() {
//     return taskloaf::unwrap(*this); 
// }
// 
data_ptr& untyped_future::get() {
    wait();
    return v.get();
}

void untyped_future::wait() {
    cur_worker->set_stopped(false);
    bool already_thenned = false;
    this->then(location::here, make_closure(
        [&] (ignore, ignore) {
            cur_worker->set_stopped(true);
            already_thenned = true;
            return ignore{};
        }
    ));
    std::cout << (int)(this->v.iv->val) << std::endl;
    if (!already_thenned) {
        cur_worker->run();
    }
}

untyped_future ready(data_ptr d) {
    untyped_future out;
    out.v.fulfill(std::move(d));
    std::cout << (int)(out.v.iv->val) << std::endl;
    return out;
}

untyped_future then(int loc, untyped_future& f, closure fnc) {
    untyped_future out_future; 
    auto iloc = internal_loc(loc);
    f.v.add_trigger(make_closure(
        [] (std::tuple<untyped_future,closure,internal_location>& d, data_ptr& v) {
            auto t = make_closure(
                [] (std::tuple<untyped_future,closure,data_ptr>& d, ignore) {
                    std::get<0>(d).v.fulfill(std::get<1>(d)(std::get<2>(d)));
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
// 
// untyped_future async(int loc, closure fnc) {
//     untyped_future out_future;
//     auto iloc = internal_loc(loc);
//     closure t(
//         [] (std::tuple<untyped_future,closure>& d, ignore) {
//             std::get<0>(d).v.fulfill(std::get<1>(d)());
//             return ignore{};
//         },
//         std::make_tuple(out_future, std::move(fnc))
//     );
//     schedule(iloc, std::move(t));
//     return out_future;
// }
// 
// untyped_future async(location loc, closure fnc) {
//     return async(static_cast<int>(loc), std::move(fnc));
// }
// 
// untyped_future async(closure fnc) {
//     return async(location::anywhere, std::move(fnc));
// }
// 
// untyped_future unwrap(untyped_future& fut) {
//     untyped_future out_future;
//     fut.v.add_trigger(closure(
//         [] (untyped_future& out_future, untyped_future& f) {
//             f.v.add_trigger(closure(
//                 [] (untyped_future& out_future, data& d) {
//                     out_future.v.fulfill(std::move(d));
//                     return ignore{};
//                 },
//                 std::move(out_future)
//             ));
//             return ignore{};
//         },
//         out_future
//     ));
//     return out_future;
// }
// 
// untyped_future when_both_leaf(untyped_future a, untyped_future b) {
//     return a.then(closure(
//         [] (untyped_future& b, Data& a_data) {
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
// untyped_future when_all(std::vector<untyped_future> fs) {
//     TLASSERT(fs.size() != 0);
//     if (fs.size() == 1) {
//         return fs[0];
//     }
//     std::vector<untyped_future> recurse_fs;
//     for (size_t i = 0; i < fs.size(); i += 2) {
//         recurse_fs.push_back(when_both(fs[i], fs[i + 1]));
//     }
//     return when_all(recurse_fs);
// }

}
