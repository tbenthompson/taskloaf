#include "launcher.hpp"
#include "default_worker.hpp"
#include "local_comm.hpp"
#include "mpi_comm.hpp"

#include <thread>
#include <atomic>

namespace taskloaf {

struct local_context_internals: public context_internals {
    std::vector<std::thread> threads;
    std::vector<std::unique_ptr<default_worker>> workers;
    std::shared_ptr<local_comm_queues> lcq;
    default_worker main_worker;
    config cfg;

    local_context_internals(size_t n_workers, config cfg):
        lcq(std::make_shared<local_comm_queues>(n_workers)),
        main_worker(std::make_unique<local_comm>(lcq, 0)),
        cfg(cfg)
    {
        set_cur_worker(&main_worker);
        main_worker.set_core_affinity(0);

        for (size_t i = 1; i < n_workers; i++) { 
            workers.emplace_back(std::make_unique<default_worker>(
                std::make_unique<local_comm>(lcq, i)
            ));
        }
        for (size_t i = 1; i < n_workers; i++) { 
            threads.emplace_back(
                [i, this] () mutable {
                    auto& w = workers[i - 1];
                    set_cur_worker(w.get());
                    w->set_core_affinity(i);
                    w->run();
                    clear_cur_worker();
                }
            );
        }
    }

    ~local_context_internals() {
        main_worker.shutdown();

        for (auto& t: threads) {
            t.join();
        }
        if (cfg.print_stats) {
            for (auto& w: workers) {
                w->my_log.write_stats(std::cout);
            }
        }
        clear_cur_worker();
    }

    local_context_internals(const context_internals&) = delete;
    local_context_internals(context_internals&&) = delete;
};

context::context(std::unique_ptr<context_internals> internals): 
    internals(std::move(internals)) 
{}
context::~context() = default;
context::context(context&&) = default;

context launch_local(size_t n_workers, config cfg) {
    return context(std::make_unique<local_context_internals>(n_workers, cfg));
}

#ifdef MPI_FOUND

struct mpi_context_internals: public context_internals {
    default_worker w; 

    mpi_context_internals():
        w(std::make_unique<mpi_comm>())
    {
        set_cur_worker(&w);
    }

    ~mpi_context_internals() {
        w.shutdown();
        clear_cur_worker();
    }
};

context launch_mpi() {
    return context(std::make_unique<mpi_context_internals>());
}

#endif

} //end namespace taskloaf
