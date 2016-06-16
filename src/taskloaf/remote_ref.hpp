#pragma once

#include "address.hpp"
#include "closure.hpp"
#include "worker.hpp"

namespace taskloaf {

template <typename T>
struct remote_ref {
    T* hdl;
    address owner;

    static T* allocate() {
        auto& pool = get_pool<sizeof(T)>();
        auto* ptr = pool.malloc();
        new (ptr) T();
        auto* out = reinterpret_cast<T*>(ptr);
        out->refs = 1;
        return out;
    }

    bool local() const {
        return owner == cur_addr;
    }

    remote_ref():
        hdl(allocate()),
        owner(cur_addr)
    {}

    remote_ref(const remote_ref& other):
        hdl(other.hdl),
        owner(other.owner)
    {
        if (local()) {
            hdl->refs++; 
        } else {
            cur_worker->add_task(owner, [hdl = this->hdl] (_,_) {
                hdl->refs++; 
                return _{};
            });
        }
    }

    remote_ref(remote_ref&& other):
        hdl(other.hdl),
        owner(other.owner)
    {
        other.hdl = nullptr;
    }

    ~remote_ref() { 
        if (hdl == nullptr) {
            return;
        }
        if (local()) {
            dec_ref(hdl);
        } else {
            if (cur_worker == nullptr) {
                return;
            }
            cur_worker->add_task(owner, [hdl = this->hdl] (_,_) { 
                dec_ref(hdl); 
                return _{};
            });
        }
    }

    static void dec_ref(T* hdl) {
        hdl->refs--; 
        if (hdl->refs == 0) {
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
