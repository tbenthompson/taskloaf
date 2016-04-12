#include "launcher.hpp"
#include "worker.hpp"
#include "local_comm.hpp"
#include "serializing_comm.hpp"

#include <thread>
#include <atomic>

namespace taskloaf {


template <typename FComm>
void helper(size_t n_workers, std::function<IVarRef()> f,
    FComm comm_builder) 
{
    std::vector<std::thread> threads;
    Address root_addr;
    std::atomic<bool> ready(false);
    for (size_t i = 0; i < n_workers; i++) { 
        threads.emplace_back(
            [f, i, comm_builder, &root_addr, &ready] () mutable {
                Worker w(comm_builder(i));
                cur_worker = &w;
                w.set_core_affinity(i);
                if (i == 0) {
                    root_addr = w.get_addr();
                    ready = true;
                    f();
                } else {
                    while (!ready) {}
                    w.introduce(root_addr); 
                }
                w.run();
            }
        );
    }

    for (auto& t: threads) { 
        t.join();         
    }
}


void launch_local_helper(size_t n_workers, std::function<IVarRef()> f) {
    auto lcq = std::make_shared<LocalCommQueues>(n_workers);
    helper(n_workers, f, [&] (size_t i) {
        return std::make_unique<LocalComm>(LocalComm(lcq, i));
    });
}
void launch_local_helper_serializing(size_t n_workers, std::function<IVarRef()> f) {
    auto lcq = std::make_shared<LocalCommQueues>(n_workers);
    helper(n_workers, f, [&] (size_t i) {
        return std::make_unique<SerializingComm>(
            std::make_unique<LocalComm>(LocalComm(lcq, i))
        );
    });
}

void launch_local_helper_singlethread(size_t n_workers, std::function<IVarRef()> f) {
    auto lcq = std::make_shared<LocalCommQueues>(n_workers);
    std::vector<std::unique_ptr<Worker>> ws(n_workers);
    for (size_t i = 0; i < n_workers; i++) { 
        auto comm = std::make_unique<SerializingComm>(
            std::make_unique<LocalComm>(LocalComm(lcq, i))
        );
        ws[i] = std::make_unique<Worker>(std::move(comm));
        cur_worker = ws[i].get();
        if (i == 0) {
            f();
        } else {
            ws[i]->introduce(ws[0]->get_addr()); 
        }
    }
    while (!ws[0]->stop) {
        for (size_t i = 0; i < n_workers; i++) { 
            cur_worker = ws[i].get();
            ws[i]->one_step();
        }
    }
}

int shutdown() {
    cur_worker->shutdown(); 
    return 0;
}

} //end namespace taskloaf
