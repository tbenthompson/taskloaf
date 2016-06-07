#include "launcher.hpp"
#include "default_worker.hpp"
#include "local_comm.hpp"
#include "mpi_comm.hpp"

#include <thread>
#include <atomic>

namespace taskloaf {

struct LocalContextInternals: public ContextInternals {
    std::vector<std::thread> threads;
    std::vector<std::unique_ptr<DefaultWorker>> workers;
    std::shared_ptr<LocalCommQueues> lcq;
    DefaultWorker main_worker;
    Config cfg;

    LocalContextInternals(size_t n_workers, Config cfg):
        lcq(std::make_shared<LocalCommQueues>(n_workers)),
        main_worker(std::make_unique<LocalComm>(lcq, 0)),
        cfg(cfg)
    {
        cur_worker = &main_worker;
        main_worker.set_core_affinity(0);

        for (size_t i = 1; i < n_workers; i++) { 
            workers.emplace_back(std::make_unique<DefaultWorker>(
                std::make_unique<LocalComm>(lcq, i)
            ));
        }
        for (size_t i = 1; i < n_workers; i++) { 
            threads.emplace_back(
                [i, this] () mutable {
                    auto& w = workers[i - 1];
                    cur_worker = w.get();
                    w->set_core_affinity(i);
                    w->run();
                    cur_worker = nullptr;
                }
            );
        }
    }

    ~LocalContextInternals() {
        main_worker.shutdown();

        for (auto& t: threads) {
            t.join();
        }
        if (cfg.print_stats) {
            for (auto& w: workers) {
                w->log.write_stats(std::cout);
            }
        }
        cur_worker = nullptr;
    }

    LocalContextInternals(const ContextInternals&) = delete;
    LocalContextInternals(ContextInternals&&) = delete;
};

Context::Context(std::unique_ptr<ContextInternals> internals): 
    internals(std::move(internals)) 
{}
Context::~Context() = default;
Context::Context(Context&&) = default;

Context launch_local(size_t n_workers, Config cfg) {
    return Context(std::make_unique<LocalContextInternals>(n_workers, cfg));
}

#ifdef MPI_FOUND

struct MPIContextInternals: public ContextInternals {
    DefaultWorker w; 

    MPIContextInternals():
        w(std::make_unique<MPIComm>())
    {
        cur_worker = &w;
    }

    ~MPIContextInternals() {
        w.shutdown();
        cur_worker = nullptr;
    }
};

Context launch_mpi() {
    return Context(std::make_unique<MPIContextInternals>());
}

#endif

} //end namespace taskloaf
