#include "taskloaf.hpp"

namespace tl = taskloaf;

auto say(std::string phrase) {
    std::cout << phrase << std::endl;
}

int main() {
    tl::launch_local(1, [] () {
        tl::when_all(
            tl::ready(std::string("HI")).then(say),
            tl::ready(std::string("BYE"))
        ).then([] (std::string yo) {
            say(yo);
        }).then(tl::shutdown);
    });
}
