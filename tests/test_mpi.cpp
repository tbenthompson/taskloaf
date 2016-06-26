#include "doctest.h"

#include "taskloaf/mpi_comm.hpp"
#include "taskloaf.hpp"

#include "serializable_functor.hpp"

#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

// Many interesting issues don't arise when using 2 processors. For example,
// a communicator will only receive messages from two different other procs
// with at least two procs. So, the 2 proc cases can mostly be considered
// to test the serialization aspects, while the 3 proc cases should 
// approximate the more general case pretty well.

using namespace taskloaf;

#define MPI_TEST_CASE_HELPER(fnc_name, n_requested_procs_arg, name_arg) \
    void fnc_name();\
    TEST_CASE(name_arg) {\
        const char* name = name_arg;\
        int n_requested_procs = n_requested_procs_arg;\
        int n_procs;\
        MPI_Comm_size(MPI_COMM_WORLD, &n_procs);\
        if (n_procs == n_requested_procs) {\
            fnc_name();\
        } else if (mpi_rank() == 0) {\
            std::cout << "Skipping test \"" << name \
                << "\" because it requires " << n_requested_procs \
                << " MPI processes." << std::endl;    \
        }\
    }\
    void fnc_name()

#define MPI_TEST_CASE(n_requested_procs, name) \
    MPI_TEST_CASE_HELPER(\
        DOCTEST_ANONYMOUS(MPI_TEST_AUTOGEN_NAME_), n_requested_procs, name\
    )

void wait_for_a_message(mpi_comm& c) {
    bool stop = false;
    while (!stop) {
        auto d = c.recv();
        if (d.empty()) {
            continue;
        }
        d(data(&c));
        stop = true;
    }
}

void wait_for_some_messages(mpi_comm& c, int times) {
    for (int i = 0; i < times; i++) {
        wait_for_a_message(c);
    }
}

void test_simple_send(closure f) {
    mpi_comm c;
    if (mpi_rank() == 0) {
        c.send({1}, f);
    } else {
        wait_for_a_message(c);
    }
}

MPI_TEST_CASE(2,"Send simple") {
    test_simple_send(closure(
        [] (int v, ignore&) { REQUIRE(v == 10); return ignore{};},
        10
    ));
}

MPI_TEST_CASE(2,"Send data") {
    test_simple_send(closure(
        [] (std::string s, ignore&) { REQUIRE(s == "HI"); return ignore{};},
        std::string("HI")
    ));
}

MPI_TEST_CASE(2,"Send lambda") {
    int b = 3;
    test_simple_send(closure(
        [] (closure& f, ignore&) {
            int x = f(data(3));
            REQUIRE(x == 9);
            return ignore{};
        },
        closure([=] (ignore&, int a) { return a * b; })
    ));
}

MPI_TEST_CASE(2,"Send functor") {
    auto f = get_serializable_functor();

    test_simple_send(closure(
        [] (closure& f, ignore&) { 
            int x = f(data(5));
            REQUIRE(x == 120);
            return ignore{};
        },
        std::move(f)
    ));
}

MPI_TEST_CASE(2,"MPI Remote task") {
    auto ctx = launch_mpi();
    
    std::string s("HI");
    if (cur_worker->get_addr() == address{0}) {

        cur_worker->add_task({1}, closure([&] (std::string s,_) {

            REQUIRE(cur_worker->get_addr() == address{1});
            REQUIRE(s == "HI");

            cur_worker->add_task({0}, closure([&] (std::string s,_) {

                REQUIRE(cur_worker->get_addr() == address{0});
                REQUIRE(s == "HI");

                cur_worker->shutdown();
                return _{};

            }, s));
            return _{};

        }, s));
        cur_worker->run();
    }
}

MPI_TEST_CASE(2,"MPI Remote task return") {
    auto ctx = launch_mpi();

    if (cur_worker->get_addr().id == 0) {
        REQUIRE(cur_worker->get_addr() == address{0});
        int x = ut_task(1, [] (_,_) {
            REQUIRE(cur_worker->get_addr() == address{1});
            return 13; 
        }).get();
        REQUIRE(x == 13);
    }
}

MPI_TEST_CASE(2,"MPI ready future as closure arg") {
    auto ctx = launch_mpi();
    int result = task(1, [] (future<int> a_fut) { 
        return a_fut.then([=] (int a) { return a + 1; }); 
    }, ready(10)).unwrap().get();
    REQUIRE(result == 11);
}

MPI_TEST_CASE(2,"MPI unready future as closure arg") {
    auto ctx = launch_mpi();
    int result = task(1, [] (future<int> a_fut) { 
        return a_fut.then([=] (int a) { return a + 1; }); 
    }, task(1, [] { return 10; })).unwrap().get();
    REQUIRE(result == 11);
}

ignore ring_handle(int x, mpi_comm* c);
void ring_send(int dest, mpi_comm* c) {
    c->send(dest % 3, closure([] (int x, mpi_comm* c) { 
        return ring_handle(x, c);
    }, dest));
}

ignore ring_handle(int x, mpi_comm* c) {
    REQUIRE(mpi_rank() == x % 3);
    ring_send(x + 1, c);
    return ignore{};
}

MPI_TEST_CASE(3, "Three procs send") {
    mpi_comm c;
    if (mpi_rank() == 0) {
        ring_send(0, &c);
        ring_send(1, &c);
        ring_send(2, &c);
        wait_for_some_messages(c, 5);
    } else if (mpi_rank() == 1) {
        wait_for_some_messages(c, 5);
    } else {
        wait_for_some_messages(c, 5);
    }
}

MPI_TEST_CASE(3, "Three procs send tasks") {
    auto ctx = launch_mpi();
    if (mpi_rank() == 0) {
        task(1, [] () { REQUIRE(cur_worker->get_addr() == address{1}); return 0; });
        task(2, [] () { REQUIRE(cur_worker->get_addr() == address{2}); return 0; });
    }
}

MPI_TEST_CASE(3, "Three procs get result") {
    auto ctx = launch_mpi();
    if (mpi_rank() == 0) {
        REQUIRE(task(1, [] () { return 10; }).get() == 10);
    }
}

MPI_TEST_CASE(3, "Three procs sum") {
    auto ctx = launch_mpi();
    if (mpi_rank() == 0) {
        auto sum = ready(1);
        for (int i = 0; i < 3; i++) {
            sum = task(i, [i] (future<int> a_fut) {
                REQUIRE(cur_worker->get_addr() == address{i});
                return a_fut.then([] (int a) {
                    return a + 1; 
                });
            }, sum).unwrap();
        }
        REQUIRE(sum.get() == 4);
    }
}
