#include "launcher.hpp"
#include "default_worker.hpp"
#include "local_comm.hpp"
#include "mpi_comm.hpp"

#include <thread>
#include <atomic>

namespace taskloaf {

struct local_context_internals: public context_internals {
    const int sleep_ms = 5;
    std::atomic<bool> stopped;
    std::unique_ptr<std::thread> interrupter;
    std::vector<std::thread> threads;
    std::vector<default_worker*> workers;
    local_comm_queues lcq;
    default_worker main_worker;
    config cfg;

    void interrupt_all_workers() {
        while (!stopped) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            for (auto& w: workers) {
                w->set_needs_interrupt(true);
            }
            main_worker.set_needs_interrupt(true);
        }
    }

    local_context_internals(size_t n_workers, config cfg):
        stopped(false),
        lcq(n_workers),
        main_worker(std::make_unique<local_comm>(lcq, 0)),
        cfg(cfg)
    {
        set_cur_worker(&main_worker);
        main_worker.set_core_affinity(0);

        if (n_workers > 1) {
            workers.resize(n_workers - 1);

            std::atomic<size_t> started_workers(0);
            for (size_t i = 1; i < n_workers; i++) { 
                threads.emplace_back(
                    [&started_workers, i, this] () mutable {
                        default_worker w(std::make_unique<local_comm>(this->lcq, i));
                        this->workers[i - 1] = &w;
                        started_workers++;
                        set_cur_worker(&w);

                        w.set_core_affinity(i);
                        w.run();

                        if (this->cfg.print_stats) {
                            w.log.write_stats(std::cout);
                        }

                        clear_cur_worker();
                    }
                );
            }
            // Wait until workers all start running.
            while (started_workers < n_workers - 1) {}
        }

        // Don't start the interrupter until after the workers have been 
        // started to avoid a data race.
        interrupter = std::make_unique<std::thread>(
            [this] () { this->interrupt_all_workers(); }
        );
    }

    ~local_context_internals() {
        stopped = true;
        main_worker.shutdown();
        interrupter->join();

        for (auto& t: threads) {
            t.join();
        }
        if (cfg.print_stats) {
            main_worker.log.write_stats(std::cout);
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
    config cfg;

    mpi_context_internals(config cfg):
        w(std::make_unique<mpi_comm>()),
        cfg(cfg)
    {
        set_cur_worker(&w);
        if (mpi_rank() != 0) {
            w.run();
        }
    }

    ~mpi_context_internals() {
        if (mpi_rank() == 0) {
            w.shutdown();
        }
        if (cfg.print_stats) {
            w.log.write_stats(std::cout);
        }
        clear_cur_worker();
    }
};

context launch_mpi(config cfg) {
    return context(std::make_unique<mpi_context_internals>(cfg));
}

#endif

} //end namespace taskloaf
