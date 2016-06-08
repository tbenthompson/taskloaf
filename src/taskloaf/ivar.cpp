#include "ivar.hpp"
#include "worker.hpp"

#include <short_alloc.h>
#include <cereal/types/memory.hpp>

namespace taskloaf {

constexpr static size_t null_handle = std::numeric_limits<size_t>::max();

template <class T, std::size_t BufSize = 128>
using SmallVector = std::vector<T, short_alloc<T, BufSize, alignof(T)>>;

template <typename T>
struct Pool {
    SmallVector<IVarData>::allocator_type::arena_type entries_arena;
    SmallVector<IVarData> entries;
    SmallVector<size_t>::allocator_type::arena_type free_arena;
    SmallVector<size_t> free;

    Pool(): entries(entries_arena), free(free_arena) {
        entries.reserve(1);
        free.reserve(1);
    }

    ~Pool() {}

    size_t allocate() {
        size_t handle;
        if (free.size() > 0) {
            handle = free.back();
            new (&entries[handle]) IVarData();
            entries[handle].references = 1;
            free.pop_back(); 
        } else {
            entries.emplace_back();
            handle = entries.size() - 1;
        }
        tlassert(entries[handle].references == 0);
        return handle;
    }

    void inc_ref(size_t handle) {
        entries[handle].references++;
    }

    void dec_ref(size_t handle) {
        tlassert(handle != null_handle);
        tlassert(entries[handle].references != 0);
        entries[handle].references--;
        if (entries[handle].references == 0) {
            entries[handle].~IVarData();
            free.push_back(handle);
        }
    }

    IVarData& get(size_t handle) {
        return entries[handle];
    }
};

static Pool<IVarData>* data_repo = new Pool<IVarData>();

Ref::Ref():
    handle(null_handle) 
{}

Ref::Ref(size_t handle):
    handle(handle)
{}

Ref::Ref(const Ref& other) {
    handle = null_handle;
    *this = other;
}

Ref& Ref::operator=(const Ref& other) {
    if (handle != null_handle) {
        data_repo->dec_ref(handle);
    }
    handle = other.handle;
    data_repo->inc_ref(handle);
    return *this;
}

Ref::~Ref() {
    data_repo->dec_ref(handle);
}

IVarData& Ref::get() {
    return data_repo->get(handle);
}

IVar::IVar():
    ref(data_repo->allocate()),
    owner(cur_addr)
{}

void IVar::add_trigger(TriggerT trigger) {
    auto action = [] (Ref ref, TriggerT& trigger) {
        auto& data = ref.get();
        if (data.fulfilled) {
            trigger(data.vals);
        } else {
            data.triggers.push_back(std::move(trigger));
        }
    };
    if (owner == cur_addr) {
        action(ref, trigger); 
    } else {
        cur_worker->add_task(owner, TaskT(
            action, ref, std::move(trigger)
        ));
    }
}

void IVar::fulfill(std::vector<Data> vals) {
    auto action = [] (Ref ref, std::vector<Data>& vals) {
        auto& data = ref.get();
        tlassert(!data.fulfilled);
        data.fulfilled = true;
        data.vals = std::move(vals);
        for (auto& t: data.triggers) {
            t(data.vals);
        }
        data.triggers.clear();
    };
    if (owner == cur_addr) {
        action(ref, vals); 
    } else {
        cur_worker->add_task(owner, TaskT(
            action, ref, std::move(vals)
        ));
    }
}

std::vector<Data> IVar::get_vals() {
    return ref.get().vals;
}

} //end namespace taskloaf
