#include "catch.hpp"

#include "taskloaf.hpp"

using namespace taskloaf;

TEST_CASE("Launch and immediately destroy", "[launch]") {
    launch_local(4);
}
