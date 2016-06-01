#include "launcher.hpp"
#include "default_worker.hpp"
#include "local_comm.hpp"
#include "serializing_comm.hpp"
#include "mpi_comm.hpp"
#include "protocol.hpp"

#include <thread>
#include <atomic>

namespace taskloaf {

struct LocalContextInternals: public ContextInternals {
    std::vector<std::thread> threads;
    std::vector<std::unique_ptr<DefaultWorker>> workers;
    std::shared_ptr<LocalCommQueues> lcq;
    DefaultWorker main_worker;

    LocalContextInternals(size_t n_workers):
        lcq(std::make_shared<LocalCommQueues>(n_workers)),
        main_worker(std::make_unique<LocalComm>(lcq, 0))
    {
        cur_worker = &main_worker;
        main_worker.set_core_affinity(0);

        Address root_addr = main_worker.get_addr();

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
                    w->introduce(main_worker.get_addr()); 
                    w->run();
                    cur_worker = nullptr;
                }
            );
        }
    }

    ~LocalContextInternals() {
        main_worker.shutdown();
        for (auto& w: workers) {
            main_worker.comm->send(w->get_addr(), Msg(
                Protocol::Shutdown, make_data(1)
            ));
        }
        for (auto& t: threads) {
            t.join();
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

Context launch_local(size_t n_workers) {
    return Context(std::make_unique<LocalContextInternals>(n_workers));
}

#ifdef MPI_FOUND

struct MPIContextInternals: public ContextInternals {
    DefaultWorker w; 

    MPIContextInternals():
        w(std::unique_ptr<SerializingComm>(new SerializingComm(
            std::unique_ptr<MPIComm>(new MPIComm())
        )))
    {
        cur_worker = &w;
        auto& endpts = w.get_comm().remote_endpoints();
        for (size_t i = 0; i < endpts.size(); i++) {
            auto& e = endpts[i];
            if (e.port < w.get_addr().port) {
                w.introduce(e);
            }
        }
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
