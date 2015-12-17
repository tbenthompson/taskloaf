#include "catch.hpp"

#include "print_tree.hpp"
#include "fib.hpp"

using namespace taskloaf;

TEST_CASE("Fib print") {
    auto f = fib(5);
    print(f);
}
