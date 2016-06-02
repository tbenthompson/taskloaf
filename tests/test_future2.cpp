#include "catch.hpp"

#include "taskloaf.hpp"

using namespace taskloaf; 

struct ABC {
    auto plan() {
        return ready(*this).then([] (ABC abc) { return abc.rocks(); });
    }

    template <typename Archive>
    void serialize(Archive&) {}

    std::string rocks() { return "jump!"; }
};

TEST_CASE("Then member fnc") {
    auto ctx = launch_local(1);
    ABC abc;
    abc.plan().wait();
}

TEST_CASE("Empty futures") {
    auto ctx = launch_local(1);
    auto f = async([] () {})
        .then([] () { return 0; })
        .then([] (int) {})
        .then([] () {});
    f.wait();
}

TEST_CASE("Return data") {
    auto ctx = launch_local(1);
    auto f = async([] () { return make_data(10); });
    REQUIRE(f.then([] (int x) { return x + 1; }).get() == 11);
}

TEST_CASE("Multiple Data return values") {
    auto ctx = launch_local(1);
    auto f = async([] () { return std::make_tuple(make_data(10), make_data(1)); });
    REQUIRE(f.then([] (int x, int y) { return x + y; }).get() == 11);
}

TEST_CASE("Multiple typed return values") {
    auto ctx = launch_local(1);
    auto f = async([] () { return std::make_tuple(10, 1); });
    REQUIRE(f.then([] (int x, int y) { return x + y; }).get() == 11);
}
