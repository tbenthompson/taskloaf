#include "catch.hpp"

#include "taskloaf/worker.hpp"
#include "taskloaf/local_comm.hpp"

using namespace taskloaf;

// struct Context {
//     Context    
// };
// (size_t n_workers) {
//     std::vector<std::thread> threads;
//     Address root_addr;
//     std::atomic<bool> ready(false);
//     for (size_t i = 0; i < n_workers; i++) { 
//         threads.emplace_back(
//             [f, i, comm_builder, &root_addr, &ready] () mutable {
//                 Worker w(comm_builder(i));
//                 cur_worker = &w;
//                 w.set_core_affinity(i);
//                 if (i == 0) {
//                     root_addr = w.get_addr();
//                     ready = true;
//                     f();
//                 } else {
//                     while (!ready) {}
//                     w.introduce(root_addr); 
//                 }
//                 w.run();
//             }
//         );
//     }
// 
//     for (auto& t: threads) { 
//         t.join();         
//     }
// }

TEST_CASE("Free launch") {
    // launch(1);

}
