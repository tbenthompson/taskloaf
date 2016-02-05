#pragma once

#include "address.hpp"
#include "fnc.hpp"
#include "id.hpp"

#include <vector>

namespace taskloaf {

typedef Function<void(std::vector<Data>& val)> TriggerT;

struct RefData {
    ID id;
    int generation = 0;
    int children = 0;
};

inline RefData copy_ref(RefData& ref) {
    ref.children++;
    return {ref.id, ref.generation + 1, 0};
}

struct ReferenceCount {
    std::vector<int> cs;

    ReferenceCount():
        cs(1, 1)
    {}

    void zero() {
        cs.clear();
    }

    void dec(const RefData& ref) {
        auto cssize = static_cast<int>(cs.size());
        if (ref.children == 0 && ref.generation >= cssize) {
            cs.resize(ref.generation + 1);
        } else if (ref.generation + 1 >= cssize) {
            cs.resize(ref.generation + 2);
        }
        cs[ref.generation]--;
        cs[ref.generation + 1] += ref.children;
        assert(cs[0] >= 0);
    }

    bool alive() {
        for (auto& c: cs) {
            if (c > 0) {
                return true;
            }
        }
        return false;
    }
};

// A reference counting "pointer" to an IVar
struct IVarRef {
    RefData data;
    bool empty;

    ID& id() {
        return data.id;
    }

    const ID& id() const {
        return data.id;
    }

    IVarRef();
    explicit IVarRef(ID id);
    IVarRef(IVarRef&&);
    IVarRef& operator=(IVarRef&&);
    IVarRef& operator=(const IVarRef&);
    IVarRef(const IVarRef&);
    ~IVarRef();
};

} //end namespace taskloaf
