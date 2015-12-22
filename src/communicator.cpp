#include "communicator.hpp"
#include "ivar_tracker.hpp"

#include <caf/all.hpp>
#include <caf/io/all.hpp>

namespace taskloaf {

using meet_atom = caf::atom_constant<caf::atom("meet_atom")>;
using inc_ref_atom = caf::atom_constant<caf::atom("inc_ref")>;
using dec_ref_atom = caf::atom_constant<caf::atom("dec_ref")>;
using fulfill_atom = caf::atom_constant<caf::atom("fulfill")>;
using add_trigger_atom = caf::atom_constant<caf::atom("add_trig")>;

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

void CAFCommunicator::meet(Address addr) {
    friends.push_back(connect((*comm).get(), addr));
    (*comm)->send(friends.back(), meet_atom::value);
}

void CAFCommunicator::handle_messages(IVarTracker& ivars, TaskCollection& tasks) {
    (void)tasks;
    while ((*comm)->has_next_message()) {
        (*comm)->receive(
            [&] (meet_atom) {
                friends.push_back(caf::actor_cast<caf::actor>(
                    (*comm)->current_sender())
                );
            },
            [&] (inc_ref_atom, IVarRef which) {
                ivars.inc_ref(which);
            },
            [&] (dec_ref_atom, IVarRef which) {
                ivars.dec_ref(which);
            },
            [&] (fulfill_atom, IVarRef which, std::vector<Data> vals) {
                ivars.fulfill(which, std::move(vals));
            },
            [&] (add_trigger_atom, IVarRef which, TriggerT trigger) {
                ivars.add_trigger(which, std::move(trigger));
            }
        );
    }
}

void CAFCommunicator::send_inc_ref(const IVarRef& which) {
    (void)which;
}

void CAFCommunicator::send_dec_ref(const IVarRef& which) {
    (void)which;
}

void CAFCommunicator::send_fulfill(const IVarRef& which, std::vector<Data> vals) {
    (void)which; (void)vals;
}

void CAFCommunicator::send_add_trigger(const IVarRef& which, TriggerT trigger) {
    (void)which; (void)trigger;
}

void CAFCommunicator::steal() { 
    
}

} //end namespace taskloaf
