#pragma once

namespace taskloaf {

struct RefOwnershipData {
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

struct DBEntry {
    bool fulfilled_here = false;
    std::vector<Data> vals;
    std::vector<TriggerT> triggers;
    RefOwnershipData ownership;
};

void run_triggers(std::vector<TriggerT>& triggers, std::vector<Data>& vals) {
    // We can't just do a for loop over the currently present triggers
    // because a trigger on a given Ref could add another trigger on
    // the same Ref.
    while (triggers.size() > 0) {
        auto t = std::move(triggers.back());
        triggers.pop_back();
        t(vals);
    }
}

// TODO: Separate ownership from storage data.
struct RefDB {
    std::unordered_map<ID,DBEntry> db;

    auto& operator[](const ID& id) {
        return db[id];
    }

    void fulfill(const ID& id, std::vector<Data> input) {
        db[id].fulfilled_here = true;
        db[id].vals = std::move(input); 
    }

    bool is_fulfilled_somewhere(const ID& id) {
        return db[id].ownership.val_locs.size() > 0;
    }

    bool is_fulfilled_here(const ID& id) const {
        return db.count(id) > 0 && db.at(id).fulfilled_here; 
    }

    bool no_triggers(const ID& id) const {
        return db.at(id).ownership.trigger_locs.size() == 0; 
    }

    void erase(const ID& id) {
        db.erase(id);
    }

    bool contains(const ID& id) const {
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

    void run_remote_triggers(const ID& id, std::vector<TriggerT> triggers) {
        tlassert(db[id].triggers.size() == 0);
        db[id].triggers = std::move(triggers);
        run_triggers(id);
    }

    void run_local_triggers(const ID& id, std::vector<Data>& input) {
        db[id].vals = input;
        run_triggers(id);
    }

    void run_triggers(const ID& id) {
        taskloaf::run_triggers(db[id].triggers, db[id].vals);
    }
};

} // end namespace taskloaf
