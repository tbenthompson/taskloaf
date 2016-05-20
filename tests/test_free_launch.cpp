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
    void add_task(TaskT f, bool) {
        std::cout << "HI!" << std::endl;
        return internal.add_task(std::move(f), true);
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
        cur_worker = &fw;
    }

    ~Context() {
        if (!moved) {
            // fw.shutdown();
            for (auto& t: threads) {
                t.join();
            }
        }
    }
};

Context launch(size_t n_workers) {
    Context c(n_workers);

    Address root_addr = c.fw.internal.get_addr();
    std::atomic<bool> ready(false);
    for (size_t i = 0; i < n_workers; i++) { 
        c.threads.emplace_back(
            [i, lcq = c.lcq, &root_addr, &ready] () mutable {
                DefaultWorker w(std::unique_ptr<LocalComm>(new LocalComm(lcq, i)));
                cur_worker = &w;
                w.set_core_affinity(i);
                w.introduce(root_addr); 
                w.run();
            }
        );
    }
    for (size_t i = 0; i < n_workers; i++) {
        c.fw.internal.recv();
    }

    return c;
}

void sleep() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Good morning!" << std::endl;
}

// TEST_CASE("Free launch") {
//     auto c = launch(2);
//     when_all(
//         async(sleep, push),
//         async(sleep, push),
//         async(sleep, push)
//     ).then([&] () { c.fw.shutdown(); });
//     std::cout << "BYE" << std::endl;
// }
