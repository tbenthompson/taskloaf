#pragma once

#include "address.hpp"
#include "closure.hpp"
#include "worker.hpp"

#include <short_alloc.h>

namespace taskloaf {

struct delete_tracker {
    std::unordered_map<void*,bool> deleted;
};

inline delete_tracker& get_delete_tracker() {
    static thread_local delete_tracker dt; 
    return dt;
};


struct intrusive_ref_count {
    using count_vector = std::vector<int,short_alloc<int,40,alignof(int)>>;

    count_vector::allocator_type::arena_type arena;
    count_vector gen_counts;

    intrusive_ref_count(): gen_counts(arena) {}

    void dec_ref(unsigned gen, unsigned children) {
        if (gen_counts.size() < gen + 2) {
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
struct remote_ref {
    T* hdl;
    unsigned generation;
    unsigned children;
    address owner;

    static T* allocate() {
        auto& pool = get_pool<sizeof(T)>();
        auto* ptr = pool.malloc();

        TL_IF_DEBUG(get_delete_tracker().deleted[ptr] = false;,)

        new (ptr) T();
        auto* out = reinterpret_cast<T*>(ptr);
        out->ref_count.gen_counts.push_back(1);
        return out;
    }

    bool local() const {
        return owner == cur_addr;
    }

    remote_ref():
        hdl(allocate()),
        generation(0),
        children(0),
        owner(cur_addr)
    {}

    remote_ref(const remote_ref& other):
        hdl(other.hdl),
        generation(other.generation + 1),
        children(0),
        owner(other.owner)
    {
        const_cast<remote_ref*>(&other)->children += 1;
    }

    remote_ref(remote_ref&& other):
        hdl(other.hdl),
        generation(other.generation),
        children(other.children),
        owner(other.owner)
    {
        other.hdl = nullptr;
    }

    ~remote_ref() { 
        if (hdl == nullptr) {
            return;
        }
        if (local()) {
            dec_ref(hdl, generation, children);
        } else {
            if (cur_worker == nullptr) {
                return;
            }
            cur_worker->add_task(owner, 
                [hdl = this->hdl, gen = this->generation, children = this->children]
                (_,_) { 
                    dec_ref(hdl, gen, children);
                    return _{};
                }
            );
        }
    }

    static void dec_ref(T* hdl, unsigned generation, unsigned children) {
        hdl->ref_count.dec_ref(generation, children);
        if (!hdl->ref_count.alive()) {
            TLASSERT(get_delete_tracker().deleted.count(hdl) > 0);
            TLASSERT(get_delete_tracker().deleted[hdl] == false);
            TL_IF_DEBUG(get_delete_tracker().deleted[hdl] = true;,)

            auto& pool = get_pool<sizeof(T)>();
            hdl->~T();
            pool.free(hdl);
        }
    }

    T& get() const {
        TLASSERT(local());
        return *hdl;
    }

    remote_ref& operator=(remote_ref&& other) = delete;
    remote_ref& operator=(const remote_ref& other) = delete;

    void save(cereal::BinaryOutputArchive& ar) const {
        (void)ar;
    }

    void load(cereal::BinaryInputArchive& ar) {
        (void)ar;
    }
};

} //end namespace taskloaf
