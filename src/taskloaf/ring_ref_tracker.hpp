#pragma once
#include "ref_tracker.hpp"
#include "global_ref.hpp"
#include "logging.hpp"
#include "comm.hpp"

namespace taskloaf {

struct RingRefTrackerInternals;

struct RingRefTracker: public RefTracker {
    std::unique_ptr<RingRefTrackerInternals> impl;

    RingRefTracker(Log& log, Comm& comm);
    RingRefTracker(RingRefTracker&&);
    ~RingRefTracker();

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


