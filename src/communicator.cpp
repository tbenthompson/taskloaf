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

CAFCommunicator::CAFCommunicator():
    comm(std::make_unique<caf::scoped_actor>())
{
    auto port = caf::io::publish(*comm, 0);
    //TODO: This needs to be changed before a distributed run will work.
    addr = {"localhost", port};
}

CAFCommunicator::~CAFCommunicator() {

}

int CAFCommunicator::n_friends() {
    return friends.size();
}

const Address& CAFCommunicator::get_addr() {
    return addr;
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

    (*comm)->send(friends[their_addr], meet_atom::value, addr);
}

caf::actor actor_from_sender(const auto& comm) {
    return caf::actor_cast<caf::actor>((*comm)->current_sender());
}

void CAFCommunicator::handle_messages(IVarTracker& ivars, TaskCollection& tasks) {
    (void)tasks;
    while ((*comm)->has_next_message()) {
        (*comm)->receive(
            [&] (meet_atom, Address their_addr) {
                friends.insert({their_addr, actor_from_sender(comm)});
            },
            [&] (steal_atom) {
                if (tasks.empty()) {
                    return;
                }
                (*comm)->send(actor_from_sender(comm), steal_atom::value, tasks.steal());
            },
            [&] (steal_atom, TaskT t) {
                tasks.add_task(std::move(t));
            },
            [&] (inc_ref_atom, Address owner, size_t id) {
                assert(owner == addr);
                IVarRef which(owner, id);
                ivars.inc_ref(which);
            },
            [&] (dec_ref_atom, Address owner, size_t id) {
                assert(owner == addr);
                IVarRef which(owner, id);
                ivars.dec_ref(which);
            },
            [&] (fulfill_atom, Address owner, size_t id, std::vector<Data> vals) {
                assert(owner == addr);
                IVarRef which(owner, id);
                ivars.fulfill(which, std::move(vals));
            },
            [&] (add_trigger_atom, Address owner, size_t id, TriggerT trigger) {
                assert(owner == addr);
                IVarRef which(owner, id);
                ivars.add_trigger(which, std::move(trigger));
            }
        );
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

void CAFCommunicator::steal() { 
    auto& from = friends.begin()->second; 
    (*comm)->send(from, steal_atom::value);
}

} //end namespace taskloaf
