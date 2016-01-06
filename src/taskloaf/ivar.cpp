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

IVarRef::IVarRef(const IVarRef& ref):
    IVarRef(ref.id)
{
    assert(!ref.moved);
}

IVarRef::IVarRef(IVarRef&& ref) {
    assert(!ref.moved);
    id = std::move(ref.id);
    ref.moved = true;
}

IVarRef& IVarRef::operator=(IVarRef&& ref) {
    assert(!ref.moved);
    id = std::move(ref.id);
    ref.moved = true;
    return *this; 
}

} //end namespace taskloaf
