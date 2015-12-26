#include "catch.hpp"

#include "taskloaf/print_tree.hpp"

#include <iostream>

using namespace taskloaf;

TEST_CASE("Fib print") {
    std::ostringstream stream;
    auto f = when_all(ready(1), ready(1)).then([] (int x, int y) { return x + y; });
    print(f, stream);
    REQUIRE(stream.str() == "Then\n  WhenAll\n    Ready\n    Ready\n");
}
