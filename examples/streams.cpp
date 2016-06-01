#include "taskloaf.hpp"
#include <cereal/types/memory.hpp>

#include <thread>
#include <chrono>

namespace tl = taskloaf;

template <typename T>
struct FList {
    typedef std::pair<T,tl::Future<FList<T>>> Internal;
    std::unique_ptr<Internal> v;

    template <typename Archive>
    void serialize(Archive& ar) { ar(v); }
};
template <typename T>
using Stream = tl::Future<FList<T>>;

FList<int> make_stream_helper(int i) {
    if (i == 0) {
        return {nullptr};
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "Computing " << i << std::endl;
        return {std::make_unique<FList<int>::Internal>(std::make_pair(
            i, 
            tl::async([=] () { return make_stream_helper(i - 1); })
        ))};
    }
}

Stream<int> make_stream(int max) {
    return tl::ready(max).then(make_stream_helper);
}

tl::Future<int> sum_stream(FList<int>& x, int accum) {
    if (x.v != nullptr) {
        accum += x.v->first;
        std::cout << "Running sum: " << accum << std::endl;
        return x.v->second.then([=] (FList<int>& x) {
            return sum_stream(x, accum); 
        }).unwrap();
    }
    return tl::ready(accum);
}

int main() {
    auto ctx = tl::launch_local(1);
    auto stream = make_stream(10);
    auto result = stream.then(
        [] (FList<int>& x) { return sum_stream(x, 0); }
    ).unwrap().get();
    std::cout << "Total: " << result << std::endl;
}
