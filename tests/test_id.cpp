#include "catch.hpp"
#include "taskloaf/id.hpp"

#include <unordered_map>

using namespace taskloaf;

TEST_CASE("Print", "[id]")
{
    ID id{1, 0};
    std::ostringstream oss;
    oss << id;
    REQUIRE(oss.str() == "1-0");
}

TEST_CASE("Hash", "[id]")
{
    std::unordered_map<ID,int> map;
    map.insert({new_id(), 0});
}

TEST_CASE("Compare", "[id]") 
{
    ID ida{0, 0};
    ID idb{1, 0};
    ID idc{2, 0};
    ID idd{2, 1};
    ID ide{2, 1};
    REQUIRE(ida < idb);
    REQUIRE(idc < idd);
    REQUIRE(ide != idc);
    REQUIRE(ide == idd);
}

TEST_CASE("Create", "[id]") 
{
    auto id = new_id();
    auto id2 = new_id();
    REQUIRE(id != id2); //This should fail once in 2^127 runs...
}
