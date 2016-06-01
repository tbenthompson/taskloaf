#include "catch.hpp"

#include "taskloaf/default_task_collection.hpp"
#include "taskloaf/logging.hpp"
#include "taskloaf/comm.hpp"

using namespace taskloaf;

struct TestingComm: public Comm {
    Address addr = {0};
    std::vector<Address> remotes = {{1}};
    Msg* cur_msg = nullptr;
    std::vector<Msg> sent;
    MsgHandlers handlers;

    const Address& get_addr() const { return addr; }
    bool has_incoming() { return false; }
    Msg& cur_message() { return *cur_msg; }
    const std::vector<Address>& remote_endpoints() { return remotes; }
    void recv() { }

    void receive_msg(Msg msg) {
        cur_msg = &msg;
        handlers.call(msg);
        cur_msg = nullptr;
    }

    void send(const Address&, Msg msg) {
        sent.push_back(msg);
    }

    void add_handler(int msg_type, std::function<void(Data)> handler) {
        handlers.add_handler(msg_type, std::move(handler));
    }
};

#define MAKE_TASK_COLLECTION(name)\
    Log log({0});\
    TestingComm comm;\
    DefaultTaskCollection name(log, comm);\

TEST_CASE("Add task") {
    MAKE_TASK_COLLECTION(tc);
    int x = 0;
    tc.add_task({[&] (std::vector<Data>&) { x = 1; }, {}});
    REQUIRE(tc.size() == 1);
    tc.run_next();
    REQUIRE(x == 1);
}

TEST_CASE("Victimized") {
    MAKE_TASK_COLLECTION(tc); 
    tc.add_task({[&] (std::vector<Data>&) {}, {}});
    REQUIRE(tc.size() == 1);
    auto tasks = tc.victimized();
    REQUIRE(tc.size() == 0);
    REQUIRE(tasks.size() == 1);
}

TEST_CASE("Receive tasks") {
    MAKE_TASK_COLLECTION(tc); 
    tc.add_task({[&] (std::vector<Data>&) {}, {}});
    auto tasks = tc.victimized();
    tc.receive_tasks(std::move(tasks));
    REQUIRE(tc.size() == 1);
}

TEST_CASE("Steal request") {
    MAKE_TASK_COLLECTION(tc);
    tc.steal();
    REQUIRE(comm.sent.size() == 1);
    tc.steal();
    REQUIRE(comm.sent.size() == 1);
}

TEST_CASE("Stealable tasks are LIFO") {
    MAKE_TASK_COLLECTION(tc);
    int x = 0;
    tc.add_task({[&] (std::vector<Data>&) { x = 1; }, {}});
    tc.add_task({[&] (std::vector<Data>&) { x *= 2; }, {}});
    tc.run_next();
    tc.run_next();
    REQUIRE(x == 1);
}

TEST_CASE("Local tasks can't be stolen") {
    MAKE_TASK_COLLECTION(tc);
    tc.add_local_task({[&] (std::vector<Data>&) {}, {}});
    REQUIRE(tc.size() == 1);
    REQUIRE(tc.victimized().size() == 0);
}

TEST_CASE("Mixed tasks run LIFO") {
    MAKE_TASK_COLLECTION(tc);
    int x = 0;
    tc.add_task({[&] (std::vector<Data>&) { x = 1; }, {}});
    tc.add_local_task({[&] (std::vector<Data>&) { x *= 2; }, {}});
    tc.run_next();
    tc.run_next();
    REQUIRE(x == 1);
}

TEST_CASE("Send remotely assigned task") {
    MAKE_TASK_COLLECTION(tc);
    int x = 0;
    tc.add_task({1}, {[&] (std::vector<Data>&) { x = 1; }, {}});
    REQUIRE(comm.sent.size() == 1);
    comm.receive_msg(comm.sent[0]);
    tc.run_next();
    REQUIRE(x == 1);
}
