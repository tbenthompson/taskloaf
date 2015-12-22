#include "communicator.hpp"

#include <caf/io/all.hpp>

namespace taskloaf {

Communicator::Communicator() {
    auto port = caf::io::publish(comm, 0);
    //TODO: This needs to be changed before a distributed run will work.
    addr = {"localhost", port};
}

const Address& Communicator::get_addr() {
    return addr;
}

void Communicator::handle_messages(IVarTracker& ivars, TaskCollection& tasks) {
    (void)ivars; (void)tasks;
}

void Communicator::send_inc_ref(const IVarRef& which) {
    (void)which;
}

void Communicator::send_dec_ref(const IVarRef& which) {
    (void)which;
}

void Communicator::send_fulfill(const IVarRef& which, std::vector<Data> val) {
    (void)which; (void)val;
}

void Communicator::send_add_trigger(const IVarRef& which, TriggerT trigger) {
    (void)which; (void)trigger;
}

void Communicator::steal() { 

}

} //end namespace taskloaf
