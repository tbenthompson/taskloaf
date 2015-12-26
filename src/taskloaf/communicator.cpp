#include "communicator.hpp"
#include "ivar_tracker.hpp"
#include "task_collection.hpp"

#include <caf/all.hpp>
#include <caf/io/all.hpp>

namespace taskloaf {

using meet_atom = caf::atom_constant<caf::atom("meet_atom")>;
using inc_ref_atom = caf::atom_constant<caf::atom("inc_ref")>;
using dec_ref_atom = caf::atom_constant<caf::atom("dec_ref")>;
using fulfill_atom = caf::atom_constant<caf::atom("fulfill")>;
using add_trigger_atom = caf::atom_constant<caf::atom("add_trig")>;
using steal_atom = caf::atom_constant<caf::atom("steal")>;
using steal_fail_atom = caf::atom_constant<caf::atom("steal_fail")>;
using shutdown_atom = caf::atom_constant<caf::atom("shutdown")>;

CAFCommunicator::CAFCommunicator():
    comm(std::make_unique<caf::scoped_actor>()),
    stealing(false)
{
    auto port = caf::io::publish(*comm, 0);
    //TODO: This needs to be changed before a distributed run will work.
    my_addr = {"localhost", port};
}

CAFCommunicator::~CAFCommunicator() {

}

int CAFCommunicator::n_friends() {
    return friends.size();
}

const Address& CAFCommunicator::get_addr() {
    return my_addr;
}

caf::actor connect(caf::blocking_actor* self, Address to_addr)
{
    auto mm = caf::io::get_middleman_actor();
    self->send(mm, caf::connect_atom::value, to_addr.hostname, to_addr.port); 

    caf::actor dest;
    bool connected = false;
    self->do_receive(
        [&](caf::ok_atom, caf::node_id&,
            caf::actor_addr& new_connection, std::set<std::string>&) 
        {
            if (new_connection == caf::invalid_actor_addr) {
                return;
            }

            dest = caf::actor_cast<caf::actor>(new_connection);
            connected = true;
        },
        [&](caf::error_atom, const std::string& errstr) {
            auto wait = 3;
            std::cout << "FAILED CONNECTION " << errstr << std::endl;
            self->delayed_send(
                mm, std::chrono::seconds(wait), caf::connect_atom::value,
                to_addr.hostname, to_addr.port
            );
        }
    ).until([&] {return connected; });

    return dest;
}

void CAFCommunicator::meet(Address their_addr) {
    if (friends.count(their_addr) > 0) {
        return;
    }
    friends.insert({their_addr, connect((*comm).get(), their_addr)});

    (*comm)->send(friends[their_addr], meet_atom::value, my_addr);
}

caf::actor actor_from_sender(const auto& comm) {
    return caf::actor_cast<caf::actor>((*comm)->current_sender());
}

bool CAFCommunicator::handle_messages(IVarTracker& ivars, TaskCollection& tasks) 
{
    bool stop = false;
    while ((*comm)->has_next_message()) {
        (*comm)->receive(
            [&] (shutdown_atom) {
                stop = true;
            },
            [&] (meet_atom, Address their_addr) {
                friends.insert({their_addr, actor_from_sender(comm)});
            },
            [&] (steal_atom, size_t n_remote_tasks) {
                if (tasks.allow_stealing(n_remote_tasks)) {
                    (*comm)->send(actor_from_sender(comm), steal_atom::value,
                        std::move(tasks.steal())
                    );
                } else {
                    (*comm)->send(actor_from_sender(comm), steal_fail_atom::value);
                }
            },
            [&] (steal_fail_atom) {
                stealing = false;
            },
            [&] (steal_atom, TaskT t) {
                stealing = false;
                tasks.stolen_task(std::move(t));
            },
            [&] (inc_ref_atom, Address owner, size_t id) {
                assert(owner == my_addr);
                IVarRef which(owner, id);
                ivars.inc_ref(which);
            },
            [&] (dec_ref_atom, Address owner, size_t id) {
                assert(owner == my_addr);
                IVarRef which(owner, id);
                ivars.dec_ref(which);
            },
            [&] (fulfill_atom, Address owner, size_t id, std::vector<Data> vals) {
                assert(owner == my_addr);
                IVarRef which(owner, id);
                ivars.fulfill(which, std::move(vals));
            },
            [&] (add_trigger_atom, Address owner, size_t id, TriggerT trigger) {
                assert(owner == my_addr);
                IVarRef which(owner, id);
                ivars.add_trigger(which, std::move(trigger));
            }
        );
    }
    return stop;
}

void CAFCommunicator::send_shutdown() {
    for (auto& f: friends) {
        (*comm)->send(f.second, shutdown_atom::value);
    }
}

void CAFCommunicator::send_inc_ref(const IVarRef& which) {
    meet(which.owner);
    (*comm)->send(friends[which.owner], inc_ref_atom::value, which.owner, which.id);
}

void CAFCommunicator::send_dec_ref(const IVarRef& which) {
    meet(which.owner);
    (*comm)->send(friends[which.owner], dec_ref_atom::value, which.owner, which.id);
}

void CAFCommunicator::send_fulfill(const IVarRef& which, std::vector<Data> vals) {
    meet(which.owner);
    (*comm)->send(
        friends[which.owner], fulfill_atom::value,
        which.owner, which.id, std::move(vals)
    );
}

void CAFCommunicator::send_add_trigger(const IVarRef& which, TriggerT trigger) {
    meet(which.owner);
    (*comm)->send(
        friends[which.owner], add_trigger_atom::value,
        which.owner, which.id, std::move(trigger)
    );
}

void CAFCommunicator::steal(size_t n_local_tasks) { 
    if (stealing || friends.size() == 0) {
        return; 
    }
    stealing = true;

    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, friends.size() - 1);
    auto item = friends.begin();
    std::advance(item, dis(gen));
    (*comm)->send(item->second, steal_atom::value, n_local_tasks);
}

} //end namespace taskloaf
