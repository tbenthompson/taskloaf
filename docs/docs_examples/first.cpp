#include "taskloaf.hpp"

#include <chrono>
#include <thread>

namespace tl = taskloaf;

void sleep() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Sleepytime over!" << std::endl;
}

int main() {
    tl::launch_local(1, [] () {
        tl::async(sleep).then(tl::shutdown);
    });
}
