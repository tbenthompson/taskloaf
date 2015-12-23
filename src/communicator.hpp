#pragma once

#include "data.hpp"
#include "ivar.hpp"

namespace caf {
    struct scoped_actor;
    struct actor;
}

namespace taskloaf {

struct IVarTracker;
struct TaskCollection;

struct CommunicatorI {
    virtual const Address& get_addr() = 0;

    virtual void meet(Address addr) = 0;
    virtual void handle_messages(IVarTracker& ivars, TaskCollection& tasks) = 0;
    virtual void send_inc_ref(const IVarRef& which) = 0;
    virtual void send_dec_ref(const IVarRef& which) = 0;
    virtual void send_fulfill(const IVarRef& which, std::vector<Data> val) = 0;
    virtual void send_add_trigger(const IVarRef& which, TriggerT trigger) = 0;
    virtual void steal() = 0;
};

struct CAFCommunicator: public CommunicatorI {
    std::unique_ptr<caf::scoped_actor> comm; 
    Address addr;
    std::map<Address,caf::actor> friends;

    CAFCommunicator();
    ~CAFCommunicator();

    int n_friends();
    const Address& get_addr() override;
    void meet(Address addr) override;
    void handle_messages(IVarTracker& ivars, TaskCollection& tasks) override;
    void send_inc_ref(const IVarRef& which) override;
    void send_dec_ref(const IVarRef& which) override;
    void send_fulfill(const IVarRef& which, std::vector<Data> vals) override;
    void send_add_trigger(const IVarRef& which, TriggerT trigger) override;
    void steal() override;
};

} //end namespace taskloaf
