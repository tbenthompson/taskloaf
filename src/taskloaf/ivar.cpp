#include "ivar.hpp"
#include "run.hpp"
#include "worker.hpp"

#include <iostream>

namespace taskloaf {

IVarRef::IVarRef(ID id):
    id(id)
{
    cur_worker->inc_ref(*this);
}

IVarRef::~IVarRef() {
    if (!moved) {
        cur_worker->dec_ref(*this);
    }
}

IVarRef::IVarRef(IVarRef&& ref) {
    id = std::move(ref.id);
    ref.moved = true;
}

IVarRef::IVarRef(const IVarRef& ref):
    IVarRef(ref.id)
{}

} //end namespace taskloaf
