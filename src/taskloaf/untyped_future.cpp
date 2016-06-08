#include "untyped_future.hpp"
#include "worker.hpp"

namespace taskloaf {

void UntypedFuture::save(cereal::BinaryOutputArchive& ar) const {
    (void)ar;
}

void UntypedFuture::load(cereal::BinaryInputArchive& ar) {
    (void)ar;
}

UntypedFuture UntypedFuture::then(Closure f) {
    return then(Loc::anywhere, std::move(f));
}

UntypedFuture UntypedFuture::then(Loc loc, Closure f) {
    return then(static_cast<int>(loc), std::move(f));
}

UntypedFuture UntypedFuture::then(int loc, Closure f) {
    return taskloaf::then(loc, *this, std::move(f));
}

UntypedFuture UntypedFuture::unwrap() {
    return taskloaf::unwrap(*this); 
}

Data UntypedFuture::get() {
    wait();
    return ivar.get();
}

void UntypedFuture::wait() {
    cur_worker->set_stopped(false);
    bool already_thenned = false;
    this->then(Loc::here, Closure(
        [&] (Data&, Data&) {
            cur_worker->set_stopped(true);
            already_thenned = true;
            return Data{};
        }
    ));
    if (!already_thenned) {
        cur_worker->run();
    }
}

UntypedFuture ready(Data d) {
    UntypedFuture out;
    out.ivar.fulfill({d});
    return out;
}

UntypedFuture then(int loc, UntypedFuture& f, Closure fnc) {
    UntypedFuture out_future; 
    auto iloc = internal_loc(loc);
    f.ivar.add_trigger(Closure(
        [] (std::tuple<UntypedFuture,Closure,InternalLoc>& d, Data& v) {
            Closure t(
                [] (std::tuple<UntypedFuture,Closure,Data>& d, Data&) {
                    std::get<0>(d).ivar.fulfill(std::get<1>(d)(std::get<2>(d)));
                    return Data{};
                },
                std::make_tuple(
                    std::move(std::get<0>(d)), std::move(std::get<1>(d)), v
                )
            );
            schedule(std::get<2>(d), std::move(t));
            return Data{};
        },
        std::make_tuple(out_future, std::move(fnc), std::move(iloc))
    ));
    return out_future;
}

UntypedFuture async(int loc, Closure fnc) {
    UntypedFuture out_future;
    auto iloc = internal_loc(loc);
    Closure t(
        [] (std::tuple<UntypedFuture,Closure>& d, Data&) {
            std::get<0>(d).ivar.fulfill(std::get<1>(d)());
            return Data{};
        },
        std::make_tuple(out_future, std::move(fnc))
    );
    schedule(iloc, std::move(t));
    return out_future;
}

UntypedFuture async(Loc loc, Closure fnc) {
    return async(static_cast<int>(loc), std::move(fnc));
}

UntypedFuture async(Closure fnc) {
    return async(Loc::anywhere, std::move(fnc));
}

UntypedFuture unwrap(UntypedFuture& fut) {
    UntypedFuture out_future;
    fut.ivar.add_trigger(Closure(
        [] (UntypedFuture& out_future, Data& d) {
            d.get_as<UntypedFuture>().ivar.add_trigger(Closure(
                [] (UntypedFuture& out_future, Data& d) {
                    out_future.ivar.fulfill(d);
                    return Data{};
                },
                std::move(out_future)
            ));
            return Data{};
        },
        out_future
    ));
    return out_future;
}
// 
// UntypedFuture when_both_leaf(UntypedFuture a, UntypedFuture b) {
//     return a.then(Closure(
//         [] (UntypedFuture& b, Data& a_data) {
//             std::vector<Data> out{a_data};
//             out.push_back(make_data(b.then(Closure(
//                 [] (std::vector<Data> a_data, std::vector<Data>& b_data) 
//                 {
//                     for (auto& b: b_data) {
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
// UntypedFuture when_all(std::vector<UntypedFuture> fs) {
//     tlassert(fs.size() != 0);
//     if (fs.size() == 1) {
//         return fs[0];
//     }
//     std::vector<UntypedFuture> recurse_fs;
//     for (size_t i = 0; i < fs.size(); i += 2) {
//         recurse_fs.push_back(when_both(fs[i], fs[i + 1]));
//     }
//     return when_all(recurse_fs);
// }

}
