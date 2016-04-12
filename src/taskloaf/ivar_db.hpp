#pragma once

namespace taskloaf {

struct IVarOwnershipData {
    ReferenceCount ref_count;
    std::set<Address> val_locs;
    std::set<Address> trigger_locs;

    template <typename Archive>
    void serialize(Archive& ar) {
        ar(ref_count);
        ar(val_locs);
        ar(trigger_locs);
    }
};

struct IVarData {
    std::vector<Data> vals;
    std::vector<TriggerT> triggers;
    IVarOwnershipData ownership;
};

void run_triggers(std::vector<TriggerT>& triggers, std::vector<Data>& vals) {
    // We can't just do a for loop over the currently present triggers
    // because a trigger on a given IVar could add another trigger on
    // the same IVar.
    while (triggers.size() > 0) {
        auto t = std::move(triggers.back());
        triggers.pop_back();
        t(vals);
    }
    triggers.clear();
}

struct IVarDB {
    std::unordered_map<ID,IVarData> db;

    auto& operator[](const ID& id) {
        return db[id];
    }

    void erase(const ID& id) {
        db.erase(id);
    }

    bool contains(const ID& id) {
        return db.count(id) > 0;
    }

    auto begin() {
        return db.begin();
    }

    auto end() {
        return db.end();
    }

    void add_val_loc(const ID& id, const Address& loc) {
        db[id].ownership.val_locs.insert(loc);
    }

    void add_trigger_loc(const ID& id, const Address& loc) {
        db[id].ownership.trigger_locs.insert(loc);
    }

    bool is_fulfilled(const ID& id) {
        return db[id].ownership.val_locs.size() > 0;
    }

    void run_remote_triggers(const ID& id, std::vector<TriggerT> triggers) {
        taskloaf::run_triggers(triggers, db[id].vals);
    }

    void run_local_triggers(const ID& id, std::vector<Data>& input) {
        taskloaf::run_triggers(db[id].triggers, input);
    }
};

} // end namespace taskloaf
