#include "catch.hpp"
#include "taskloaf/ivar.hpp"
#include "taskloaf/address.hpp"
#include "taskloaf/local_comm.hpp"
#include "taskloaf/worker.hpp"
#include "taskloaf/future.hpp"
#include <functional>

using namespace taskloaf;

void testbed_helper(size_t n_workers, std::function<IVarRef()> f) {
    Address root_addr;
    auto lcq = std::make_shared<LocalCommQueues>(n_workers);
    std::vector<std::unique_ptr<Worker>> ws(n_workers);
    for (size_t i = 0; i < n_workers; i++) { 
        auto comm = std::make_unique<LocalComm>(LocalComm(lcq, i));
        ws[i] = std::make_unique<Worker>(std::move(comm));
        cur_worker = ws[i].get();
        if (i == 0) {
            root_addr = ws[i]->get_addr();
            f();
        } else {
            ws[i]->introduce(root_addr); 
        }
    }
    while (!ws[0]->stop) {
        for (size_t i = 0; i < n_workers; i++) { 
            cur_worker = ws[i].get();
            ws[i]->one_step();
        }
    }
}

template <typename F>
void launch_testbed(size_t n_workers, F&& f) {
    testbed_helper(n_workers, [f = std::forward<F>(f)] () {
        auto t = ready(f()).unwrap();
        return t.ivar;
    });
}





int fib_serial(int index) {
    if (index < 3) {
        return 1;
    } else {
        return fib_serial(index - 1) + fib_serial(index - 2);
    }
}


Future<int> fib(int index, int grouping = 3) {
    if (index < grouping) {
        return async([=] () { return fib_serial(index); });
    } else {
        auto af = fib(index - 1, grouping);
        auto bf = fib(index - 2, grouping);
        return when_all(af, bf).then(
            [] (int a, int b) { return a + b; }
        );
    }
}

TEST_CASE("Fib") {
    size_t n = 20;
    size_t grouping = 15;
    launch_testbed(10, [&] () {
        auto fut = fib(n, grouping);
        return fut.then([&] (int x) {
            std::cout << "fib(" << n << ") = " << x << std::endl;
            return shutdown();
        });
    });
}
