#include "taskloaf.hpp"
#include "timing.hpp"

#include <random>
#include <iostream>

using namespace taskloaf;

// TODO: Needs reduce skeleton
Future<double> sum(int lower, int upper, const std::vector<Future<double>>& vals) {
    if (lower == upper) {
        return vals[lower];
    } else {
        auto middle = (lower + upper) / 2;
        return when_all(
            sum(lower, middle, vals),
            sum(middle + 1, upper, vals)
        ).then([] (double a, double b) { return a + b; });
    }
}

Future<double> sum(const std::vector<Future<double>>& vals) {
    return sum(0, vals.size() - 1, vals);
}

double random(double a, double b) {
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(a, b);
    return dis(gen);
}

std::vector<double> random_list(size_t N, double a, double b) {
    std::vector<double> es(N);
    for (size_t i = 0; i < N; ++i) {
        es[i] = random(a, b);
    }
    return es;
}

int main() {
    int n = 10000000;
    int n_blocks = 100;
    int n_per_block = n / n_blocks;
    for (int n_workers = 1; n_workers <= 4; n_workers++) {
        TIC;
        launch(n_workers, [=] () {
            std::vector<Future<double>> chunks;
            for (int i = 0; i < n_blocks; i++) {
                chunks.push_back(async([=] () {
                    double out = 0;               
                    for (int i = 0; i < n_per_block; i++) {
                        out += random(0, 1) * random(0, 1);
                    }
                    return out;
                }));
            }
            return sum(chunks).then([] (double result) {
                std::cout << result << std::endl;   
                return shutdown();
            });
        });
        TOC("dot product " + std::to_string(n_workers));
    }
}
