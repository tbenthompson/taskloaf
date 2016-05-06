#include "taskloaf.hpp"

namespace tl = taskloaf;

void sleep() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

auto say(std::string phrase) {
    std::cout << phrase << std::endl;
}

int main() {
    tl::launch_local(1, [] () {
        tl::ready(std::string("HI"))
            .then(say)
            .then(sleep)
            .then([] () { say("BYE"); })
            .then(tl::shutdown);
    });
}
