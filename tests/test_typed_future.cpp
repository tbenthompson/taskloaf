#include "taskloaf/launcher.hpp"
#include "taskloaf/untyped_future.hpp"

namespace taskloaf {

template <typename... Ts>
struct Future {
    UntypedFuture internal;

    UntypedFuture untyped() {
        return internal;
    }
};

template <typename T>
auto ready(T&& v) {
    return Future<T>{ready({ensure_data(std::forward<T>(v))})};
}

// template <typename T, typename F>
// auto then(int loc, Future<T

} //end namespace taskloaf

#include "catch.hpp"

using namespace taskloaf;

TEST_CASE("Ready") {
    auto ctx = launch_local(2);
    int x = ready(2).untyped().get();
    REQUIRE(x == 2);
}

TEST_CASE("Then") {

}
