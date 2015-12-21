#pragma once

namespace taskloaf {

struct Address {
    //TODO: Consider changing std::string to something else for optimization.
    std::string hostname;
    uint16_t port;
};

struct Communicator {
    void inc_ref(const IVarRef& which) {
    }
    void dec_ref(const IVarRef& which) {
    }
    void fulfill(const IVarRef& which, std::vector<Data> val) {
    }
    void add_trigger(const IVarRef& which, TriggerT trigger) {
    }
    void steal() { 
    }
};

} //end namespace taskloaf
