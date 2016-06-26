#include "taskloaf.hpp"
#include "taskloaf/timing.hpp"

#include <iostream>

using namespace taskloaf;

int gcd(int a, int b) {
    if (b == 0) {
        return a;
    } else {
        return gcd(b, a % b);
    }
}

int totient(int index) {
    int result = 0;
    for (int i = 0; i < index; i++) {
        if (gcd(index, i) == 1) {
            result++;
        }
    }
    return result;
}

future<int> sum_totient(int lower, int upper) {
    if (lower == upper) {
        return ready(lower).then(totient);
    } else {
        auto middle = (lower + upper) / 2;

        auto b_fut = task([=] { return sum_totient(middle + 1, upper); }).unwrap();

        return sum_totient(lower, middle)
            .then([=] (future<int> b_fut, int a) {
                return b_fut.then([=] (int b) { return a + b; });
            }, b_fut)
            .unwrap();
    }
}

int main() {
    int n = 5000;

    for (size_t i = 1; i <= 6; i++) {
        TIC
        auto ctx = launch_local(i);
        auto x = sum_totient(1, n).get();
        std::cout << x << std::endl;
        TOC(std::to_string(i));
    }
}
