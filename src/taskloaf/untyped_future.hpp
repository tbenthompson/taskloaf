#pragma once
#include "location.hpp"
#include "ivar.hpp"

namespace taskloaf {

using ThenTaskT = Closure<std::vector<Data>(std::vector<Data>&)>;
using AsyncTaskT = Closure<std::vector<Data>()>;

struct UntypedFuture;

UntypedFuture ready(std::vector<Data> d);
UntypedFuture then(int loc, UntypedFuture& f, ThenTaskT fnc);
UntypedFuture async(int loc, AsyncTaskT fnc);
UntypedFuture async(Loc loc, AsyncTaskT fnc);
UntypedFuture async(AsyncTaskT fnc);
UntypedFuture unwrap(UntypedFuture& fut);

struct UntypedFuture {
    IVar ivar;

    void save(cereal::BinaryOutputArchive& ar) const;
    void load(cereal::BinaryInputArchive& ar);

    UntypedFuture then(ThenTaskT f);
    UntypedFuture then(Loc loc, ThenTaskT f);
    UntypedFuture then(int loc, ThenTaskT f);
    UntypedFuture unwrap();
    std::vector<Data> get();
    void wait();
};

inline UntypedFuture when_both(UntypedFuture a, UntypedFuture b) {
    return a.then(ThenTaskT(
        [] (UntypedFuture& b, std::vector<Data>& a_data) {
            return std::vector<Data>({make_data(b.then(ThenTaskT(
                [] (std::vector<Data> a_data,
                    std::vector<Data>& b_data) 
                {
                    a_data.insert(a_data.end(), b_data.begin(), b_data.end());
                    return a_data;
                },
                a_data
            )))});
        },
        std::move(b)
    )).unwrap();
}

inline UntypedFuture when_all(std::vector<UntypedFuture> fs) {
    tlassert(fs.size() != 0);
    if (fs.size() == 1) {
        return fs[0];
    }
    std::vector<UntypedFuture> recurse_fs;
    for (size_t i = 0; i < fs.size(); i += 2) {
        recurse_fs.push_back(when_both(fs[i], fs[i + 1]));
    }
    return when_all(recurse_fs);
}

} //end namespace taskloaf
