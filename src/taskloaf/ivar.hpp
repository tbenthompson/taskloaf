#pragma once
#include "closure.hpp"
#include "address.hpp"
#include "worker.hpp"

namespace taskloaf {

struct ivar_data {
    data val;
    closure triggers;
    size_t refs = 1;
};

struct ivar_db;
extern __thread ivar_db* cur_ivar_db;

struct ivar_db {
    std::vector<ivar_data> ivar_list;
    bool destroying = false;

    ivar_db() {
        cur_ivar_db = this;
    }

    ~ivar_db() {
        destroying = true;
        ivar_list.clear();
        cur_ivar_db = nullptr;
    }

    size_t new_ivar() {
        ivar_list.emplace_back();
        return ivar_list.size() - 1;
    }

    size_t size() {
        return ivar_list.size();
    }

    void inc_ref(size_t handle) {
        refs(handle)++;
    }

    void dec_ref(size_t handle) {
        if (destroying) {
            return;
        }
        TLASSERT(refs(handle) > 0);
        refs(handle)--;
        if (handle == ivar_list.size() - 1) {
            size_t resize_to = handle + 1;
            while (resize_to >= 1 && refs(resize_to - 1) == 0) {
                resize_to--;
            }
            ivar_list.resize(resize_to);
        }
    }

    size_t& refs(size_t handle) {
        TLASSERT(handle < ivar_list.size());
        return ivar_list[handle].refs;
    }

    ivar_data& get(size_t handle) {
        TLASSERT(handle < ivar_list.size());
        return ivar_list[handle]; 
    }
};

constexpr static size_t null_handle = std::numeric_limits<size_t>::max();

struct handle {
    size_t h;

    bool is_null() const { return h == null_handle; }
    void inc_ref() const { cur_ivar_db->inc_ref(h); }
    void dec_ref() const { cur_ivar_db->dec_ref(h); }
    ivar_data& get() const { return cur_ivar_db->get(h); }
};

struct remote_ref {
    handle hdl;
    address owner;

    bool local() const {
        return owner == cur_addr;
    }

    bool is_null() const {
        return hdl.is_null();
    }

    remote_ref():
        hdl({cur_ivar_db->new_ivar()}),
        owner(cur_addr)
    {}

    remote_ref(const remote_ref& other):
        hdl(other.hdl),
        owner(other.owner)
    {
        if (local()) {
            hdl.inc_ref();
        } else {
            cur_worker->add_task(owner, [hdl = this->hdl] (_,_) {
                hdl.inc_ref(); 
                return _{};
            });
        }
    }

    ~remote_ref() { 
        if (hdl.is_null()) {
            return;
        }
        if (local()) {
            hdl.dec_ref();
        } else {
            cur_worker->add_task(owner, [hdl = this->hdl] (_,_) { 
                hdl.dec_ref(); 
                return _{};
            });
        }
    }

    ivar_data& get() const {
        TLASSERT(local());
        return hdl.get();
    }

    remote_ref(remote_ref&& other) = delete;
    //     hdl(other.hdl),
    //     owner(other.owner)
    // {
    //     other.hdl = {null_handle};
    // }

    remote_ref& operator=(remote_ref&& other) = delete;
    remote_ref& operator=(const remote_ref& other) = delete;

    void save(cereal::BinaryOutputArchive& ar) const {
        (void)ar;
    }

    void load(cereal::BinaryInputArchive& ar) {
        (void)ar;
    }
};

struct ivar {
    remote_ref rr;

    bool fulfilled_here() const;
    void add_trigger(closure trigger);
    void fulfill(data vals);
    data get();
};

} //end namespace taskloaf
