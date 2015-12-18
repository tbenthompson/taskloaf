#include "catch.hpp"

#include "print_tree.hpp"
#include "fib.hpp"

#include <iostream>

using namespace taskloaf;

TEST_CASE("Fib print") {
    std::ostringstream stream;
    auto f = fib(3);
    print(f, stream);
    REQUIRE(stream.str() == "Then\n  WhenAll\n    Ready\n    Ready\n");
}
