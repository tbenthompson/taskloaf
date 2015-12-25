#include "run.hpp"

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

int main() {
    int sum = 0;
    Worker w;
    for (int i = 0; i < 10000; i++) {
        auto t = async([i] () { return totient(i); });
        run(t, w);
        // sum += t;
    }
    std::cout << sum << std::endl;
    // int n = 1000;
    // int chunks = 4;
    // for (int i = 0; i < chunks; i++) {
    //     // async(gen_vals
    // }
}
