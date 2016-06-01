#pragma once

#include "global_ref.hpp"
#include "fnc.hpp"
#include "address.hpp"
#include "closure.hpp"

namespace taskloaf {

struct ID;
struct Data;
struct Comm;
struct Log;

struct IVarTrackerInternals;

struct IVarTrackerI {
    virtual void introduce(Address addr) = 0;
    virtual void fulfill(const GlobalRef& gref, std::vector<Data> vals) = 0;
    virtual void add_trigger(const GlobalRef& gref, TriggerT trigger) = 0;
    virtual void dec_ref(const GlobalRef& gref) = 0;

    virtual bool is_fulfilled_here(const GlobalRef& gref) const = 0;
    virtual std::vector<Data> get_vals(const GlobalRef& gref) const = 0;
};

struct IVarTracker: public IVarTrackerI {
    std::unique_ptr<IVarTrackerInternals> impl;

    IVarTracker(Log& log, Comm& comm);
    IVarTracker(IVarTracker&&);
    ~IVarTracker();

    void introduce(Address addr) override;
    void fulfill(const GlobalRef& gref, std::vector<Data> vals) override;
    void add_trigger(const GlobalRef& gref, TriggerT trigger) override;
    void dec_ref(const GlobalRef& gref) override;

    bool is_fulfilled_here(const GlobalRef& gref) const override;
    std::vector<Data> get_vals(const GlobalRef& gref) const override;

    size_t n_owned() const;
    size_t n_triggers_here() const;
    size_t n_vals_here() const;
    const std::vector<ID>& get_ring_locs() const;
    std::vector<Address> ring_members() const;
};


} //end namespace taskloaf
