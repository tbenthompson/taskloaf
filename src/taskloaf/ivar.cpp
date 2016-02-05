#include "ivar.hpp"
#include "worker.hpp"

#include <iostream>

namespace taskloaf {

IVarRef::IVarRef():
    empty(true)
{}

IVarRef::IVarRef(ID id):
    data{id, 0, 0},
    empty(false)
{}

IVarRef::~IVarRef() {
    if (!empty) {
        cur_worker->dec_ref(*this);
    }
}

IVarRef::IVarRef(IVarRef&& ref) {
    assert(!ref.empty);
    ref.empty = true;
    data = std::move(ref.data);
    empty = false;
}

IVarRef& IVarRef::operator=(IVarRef&& ref) {
    assert(!ref.empty);
    ref.empty = true;
    id() = std::move(ref.id());
    empty = false;
    return *this; 
}

IVarRef::IVarRef(const IVarRef& ref) {
    *this = ref;
}

IVarRef& IVarRef::operator=(const IVarRef& ref) {
    data = copy_ref(*const_cast<RefData*>(&ref.data));
    empty = ref.empty;
    return *this;
}

} //end namespace taskloaf
