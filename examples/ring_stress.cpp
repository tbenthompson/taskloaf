#include "timing.hpp"

#include "taskloaf/ring.hpp"
#include "taskloaf/caf_comm.hpp"

#include <cmath>
#include <thread>
#include <atomic>

using namespace taskloaf;

struct RingRunner {
    const int n_locs = 3;
    std::atomic<bool> shutdown;
    CAFComm c;
    Ring r;

    RingRunner():
        shutdown(false),
        r(c, n_locs)
    {}

    RingRunner(Address addr):
        shutdown(false),
        r(c, addr, n_locs)
    {}

    std::thread run() {
        return std::thread([&] () {
            while(!shutdown) {
                c.recv();
            }
        });
    }
};

std::chrono::high_resolution_clock::time_point runner() {
    const int n_rings = 6; 
    int n_vals = 1000000;

    TIC;
    auto ids = new_ids(n_vals);
    auto vals = new_ids(n_vals);
    TOC("Make IDs");

    TIC2;
    std::vector<std::thread> threads;
    std::array<RingRunner,n_rings> rings;

    // Introduce the rings to each other.
    for (int i = 0; i < n_rings; i++) {
        if (i != 0) {
            rings[i].r.introduce(rings[0].c.get_addr());
        }
    }

    // Meet the other nodes.
    for (int j = 0; j < 10; j++) {
        for (int i = 0; i < n_rings; i++) {
            rings[i].c.recv();
        }
    }
    TOC("Launch");
    
    // Schedule the set and get operations
    TIC2;
    std::atomic<int> counter(0);
    for (int i = 0; i < n_rings; i++) {
        auto start_idx = i * std::ceil((double)n_vals / n_rings);
        auto end_idx = (i + 1) * std::ceil((double)n_vals / n_rings);
        for (int j = start_idx; j < end_idx; j++) {
            rings[i].r.set(ids[j], vals[j]);
            rings[i].r.get(ids[j], [&rings, &counter, n_vals] (Data) {
                counter++;
                if (counter == n_vals) {
                    for(auto& r: rings) {
                        r.shutdown = true; 
                    }
                }
            });
        }
    }
    TOC("Submit");

    // Run!
    TIC2;
    for (int i = 0; i < n_rings; i++) {
        threads.push_back(rings[i].run());
    }
    for (int i = 0; i < n_rings; i++) {
        threads[i].join();
    }
    TOC("Run");
    TIC2;
    return start;
}

int main() {
    auto start = runner();
    int time_ms;
    TOC("destructors");
}
