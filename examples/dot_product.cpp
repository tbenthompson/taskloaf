#include "taskloaf.hpp"
#include "taskloaf/timing.hpp"

#include <cereal/types/vector.hpp>

#include <random>
#include <iostream>

using namespace taskloaf;

double random(double a, double b) {
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(a, b);
    return dis(gen);
}

template <typename T, typename F>
taskloaf::future<T> reduce(int lower, int upper,
    std::vector<taskloaf::future<T>> vals, F f) 
{
    if (lower == upper) {
        return vals[lower];
    } else {
        return taskloaf::task(
            [lower,upper] (std::vector<taskloaf::future<T>>& vals, F& f) {
                auto middle = (lower + upper) / 2;
                auto r1 = reduce(lower, middle, vals, f);
                auto r2 = reduce(middle + 1, upper, vals, f);
                return r2.then([] (taskloaf::future<T>& val1, F& f, T& val2) {
                    return val1.then([] (F& f, T& val2, T& val1) { 
                        return f(val1, val2);
                    }, f, val2);
                }, r1, f).unwrap();
            },
            std::move(vals),
            std::move(f)
        ).unwrap();
    }
}

template <typename T, typename F>
taskloaf::future<T> reduce(const std::vector<taskloaf::future<T>>& vals, const F& f) {
    return reduce(0, vals.size() - 1, vals, f);
}

int main() {
    int n = 10000000;
    for (int n_workers = 1; n_workers <= 6; n_workers++) {
        TIC;
        auto ctx = launch_local(n_workers);
        std::vector<future<double>> chunks;
        int n_per_block = n / n_workers;
        for (int i = 0; i < n_workers; i++) {
            chunks.push_back(task(i, [=] () {
                double out = 0;               
                for (int i = 0; i < n_per_block; i++) {
                    out += random(0, 1) * random(0, 1);
                }
                return out;
            }));
        }
        reduce(chunks, std::plus<double>()).get();
        TOC("dot product " + std::to_string(n_workers));
    }
}
