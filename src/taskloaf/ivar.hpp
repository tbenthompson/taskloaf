#pragma once
#include "closure.hpp"
#include "address.hpp"
#include "worker.hpp"

#include <boost/pool/pool.hpp>
#include <boost/intrusive_ptr.hpp>

namespace taskloaf {

using TriggerT = Closure<void(std::vector<Data>&)>;
using TaskT = Closure<void()>;

struct IVarData;

struct Ref {
    size_t handle;
    
    Ref();
    Ref(size_t handle); 
    Ref(const Ref&);
    Ref& operator=(const Ref&);
    ~Ref();

    IVarData& get();

    void save(cereal::BinaryOutputArchive& ar) const {
        ar(handle);
    }
    void load(cereal::BinaryInputArchive& ar) {
        ar(handle);
    }
};

struct IVarData {
    bool fulfilled = false;
    Data val;
    std::vector<TriggerT> triggers;
    size_t references = 1;

    void save(cereal::BinaryOutputArchive& ar) const {
        (void)ar;
    }

    void load(cereal::BinaryInputArchive& ar) {
        (void)ar;
    }
};

struct IVar {
    Ref ref;
    Address owner;

    IVar();

    void add_trigger(TriggerT trigger);
    void fulfill(std::vector<Data> vals);
    std::vector<Data> get_vals();
};

} //end namespace taskloaf
