#include "run.hpp"
#include "timing.hpp"

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

Future<int> sum_totient(int lower, int upper) {
    if (lower == upper) {
        return async([=] () { return totient(lower); });
    } else {
        auto middle = (lower + upper) / 2;
        return when_all(
            sum_totient(lower, middle),
            sum_totient(middle + 1, upper)
        ).then([] (int a, int b) { return a + b; });
    }
}

int main() {
    int n = 1000;
    TIC
    launch(1, [=] () {
        return sum_totient(1, n).then([] (int x) {
            std::cout << x << std::endl;
            return shutdown();
        });
    });
    TOC("1");
    TIC2
    launch(4, [=] () {
        return sum_totient(1, n).then([] (int x) {
            std::cout << x << std::endl;
            return shutdown();
        });
    });
    TOC("4");
}
