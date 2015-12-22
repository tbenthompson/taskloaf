#pragma once

#include "data.hpp"
#include "ivar.hpp"

#include <caf/all.hpp>

namespace taskloaf {

struct IVarTracker;
struct TaskCollection;

struct CommunicatorI {
    virtual const Address& get_addr() = 0;
    virtual void handle_messages(IVarTracker& ivars, TaskCollection& tasks) = 0;
    virtual void send_inc_ref(const IVarRef& which) = 0;
    virtual void send_dec_ref(const IVarRef& which) = 0;
    virtual void send_fulfill(const IVarRef& which, std::vector<Data> val) = 0;
    virtual void send_add_trigger(const IVarRef& which, TriggerT trigger) = 0;
    virtual void steal() = 0;
};

struct Communicator: public CommunicatorI {
    //TODO: Could make this a pointer to not infect other modules with caf includes
    caf::scoped_actor comm; 
    Address addr;

    Communicator();

    const Address& get_addr() override;
    void handle_messages(IVarTracker& ivars, TaskCollection& tasks) override;
    void send_inc_ref(const IVarRef& which) override;
    void send_dec_ref(const IVarRef& which) override;
    void send_fulfill(const IVarRef& which, std::vector<Data> val) override;
    void send_add_trigger(const IVarRef& which, TriggerT trigger) override;
    void steal() override;
};

} //end namespace taskloaf
