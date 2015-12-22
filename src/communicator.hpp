#pragma once

#include "data.hpp"
#include "ivar.hpp"

#include <caf/all.hpp>

namespace taskloaf {

struct Communicator {
    caf::scoped_actor comm; 
    Address addr;

    Communicator();

    const Address& get_addr();
    void inc_ref(const IVarRef& which);
    void dec_ref(const IVarRef& which);
    void fulfill(const IVarRef& which, std::vector<Data> val);
    void add_trigger(const IVarRef& which, TriggerT trigger);
    void steal();
};

} //end namespace taskloaf
