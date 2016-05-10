#include "catch.hpp"

#include "taskloaf/default_worker.hpp"
#include "taskloaf/local_comm.hpp"
#include "taskloaf/future.hpp"

using namespace taskloaf;

struct FreeWorker: public Worker {
    DefaultWorker internal;

    FreeWorker(std::unique_ptr<Comm> p_comm):
        internal(std::move(p_comm))
    {}

    FreeWorker(FreeWorker&&) = default;

    void shutdown() {
        internal.shutdown();
    }
    bool can_compute_immediately() {
        return internal.can_compute_immediately();
    }
    size_t n_workers() const {
        return internal.n_workers();
    }
    void add_task(TaskT f) {
        return internal.add_task(std::move(f));
    }
    void fulfill(const IVarRef& ivar, std::vector<Data> vals) {
        return internal.fulfill(ivar, std::move(vals));
    }
    void add_trigger(const IVarRef& ivar, TriggerT trigger) {
        return internal.add_trigger(ivar, std::move(trigger));
    }
    void dec_ref(const IVarRef& ivar) {
        return internal.dec_ref(ivar);
    }
};

struct Context {
    std::vector<std::thread> threads;
    std::shared_ptr<LocalCommQueues> lcq;
    FreeWorker fw;
    bool moved = false;

    Context(size_t n_workers):
        lcq(std::make_shared<LocalCommQueues>(n_workers + 1)),
        fw(std::unique_ptr<LocalComm>(new LocalComm(lcq, n_workers)))
    {
        cur_worker = &fw;
    }

    Context(Context&& other):
        threads(std::move(other.threads)),
        lcq(std::move(other.lcq)),
        fw(std::move(other.fw))
    {
        other.moved = true;
    }

    ~Context() {
        if (!moved) {
            fw.shutdown();
            for (auto& t: threads) {
                t.join();
            }
        }
    }
};

Context launch(size_t n_workers) {
    Context c(n_workers);

    Address root_addr;
    std::atomic<bool> ready(false);
    for (size_t i = 0; i < n_workers; i++) { 
        c.threads.emplace_back(
            [i, lcq = c.lcq, &root_addr, &ready] () mutable {
                DefaultWorker w(std::unique_ptr<LocalComm>(new LocalComm(lcq, i)));
                cur_worker = &w;
                w.set_core_affinity(i);
                if (i == 0) {
                    root_addr = w.get_addr();
                    ready = true;
                } else {
                    while (!ready) {}
                    w.introduce(root_addr); 
                }
                w.run();
            }
        );
    }

    return c;
}

TEST_CASE("Free launch") {
    // auto c = launch(1);
    // async([] () { 
    //     std::cout << "HI!" << std::endl; 
    // });
    // std::cout << "BYE" << std::endl;
}
