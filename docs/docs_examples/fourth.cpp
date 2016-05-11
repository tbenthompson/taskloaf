#include "taskloaf.hpp"

#include <chrono>
#include <thread>

namespace tl = taskloaf;

void sleep() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Good morning!" << std::endl;
}

int main() {
    tl::launch_local(3, [] () {
        tl::when_all(
            tl::async(sleep, tl::push),
            tl::async(sleep, tl::push),
            tl::async(sleep, tl::push)
        ).then(tl::shutdown);
    });
}
