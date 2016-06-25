#pragma once

#include "address.hpp"
#include "closure.hpp"
#include "worker.hpp"

#include <short_alloc.h>
#include <thread>

namespace taskloaf {

extern thread_local std::unordered_map<void*,bool> delete_tracker; 

struct intrusive_ref_count {
    using count_vector = std::vector<int,short_alloc<int,40,alignof(int)>>;

    count_vector::allocator_type::arena_type arena;
    count_vector gen_counts;

    intrusive_ref_count(): gen_counts(arena) {}

    void dec_ref(int gen, int children) {
        if (int(gen_counts.size()) < gen + 2) {
            gen_counts.resize(gen + 2);
        }
        gen_counts[gen]--;
        gen_counts[gen + 1] += children;
    }

    bool alive() {
        for (size_t i = 0; i < gen_counts.size(); i++) {
            if (gen_counts[i] != 0) {
                return true;
            }
        }
        return false;
    }
};

template <typename T>
struct ref_internal {
    T* hdl;
    int generation;
    int children;

    ref_internal<T> copy() {
        children++;
        return {hdl, generation + 1, 0};
    }

    void dec_ref() {
        hdl->ref_count.dec_ref(generation, children);
        if (!hdl->ref_count.alive()) {
            TLASSERT(delete_tracker.count(hdl) > 0);
            TLASSERT(delete_tracker[hdl] == false);
            TL_IF_DEBUG(delete_tracker[hdl] = true;,)
            delete hdl;
        }
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        ar(reinterpret_cast<intptr_t>(hdl));
        ar(generation);
        ar(children);
    }

    void load(cereal::BinaryInputArchive& ar) {
        intptr_t hdl_in;
        ar(hdl_in);
        hdl = reinterpret_cast<T*>(hdl_in);
        ar(generation);
        ar(children);
    }

    static ref_internal allocate() {
        T* out = new T();
        TL_IF_DEBUG(delete_tracker[(void*)out] = false;,)
        out->ref_count.gen_counts.push_back(1);
        return {out, 0, 0};
    }
};

template <typename T>
struct remote_ref {
    ref_internal<T> internal;
    address owner;

    bool local() const {
        return owner == cur_worker->get_addr();
    }

    remote_ref():
        internal(ref_internal<T>::allocate()),
        owner(cur_worker->get_addr())
    {}

    remote_ref(const remote_ref& other):
        internal(const_cast<remote_ref*>(&other)->internal.copy()),
        owner(other.owner)
    {}

    remote_ref(remote_ref&& other):
        internal(std::move(other.internal)),
        owner(other.owner)
    {
        other.internal.hdl = nullptr;
    }

    remote_ref& operator=(remote_ref&& other) {
        destroy();
        internal = std::move(other.internal);
        owner = std::move(other.owner);
        other.internal.hdl = nullptr;
        return *this;
    }
        
    remote_ref& operator=(const remote_ref& other) {
        destroy();
        internal = const_cast<remote_ref*>(&other)->internal.copy();
        owner = other.owner;
        return *this;
    }

    ~remote_ref() { 
        destroy();
    }

    void destroy() {
        if (cur_worker == nullptr || internal.hdl == nullptr) {
            return;
        }
        if (local()) {
            internal.dec_ref();
        } else {
            cur_worker->add_task(owner, [internal = this->internal] (_,_) mutable {
                internal.dec_ref();
                return _{}; 
            });
        }
    }

    T& get() const {
        TLASSERT(local());
        return *internal.hdl;
    }

    void save(cereal::BinaryOutputArchive& ar) const {
        ar(const_cast<ref_internal<T>*>(&internal)->copy());
        ar(owner);
    }

    void load(cereal::BinaryInputArchive& ar) {
        ar(internal);
        ar(owner);
    }

};

} //end namespace taskloaf
