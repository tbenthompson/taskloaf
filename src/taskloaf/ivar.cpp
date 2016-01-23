#include "ivar.hpp"
#include "worker.hpp"

#include <iostream>

namespace taskloaf {

IVarRef::IVarRef():
    empty(true)
{}

IVarRef::IVarRef(ID id):
    id(id),
    empty(false)
{
    cur_worker->inc_ref(*this);
}

IVarRef::~IVarRef() {
    if (!empty) {
        cur_worker->dec_ref(*this);
    }
}

IVarRef::IVarRef(IVarRef&& ref) {
    assert(!ref.empty);
    id = std::move(ref.id);
    ref.empty = true;
    empty = false;
}

IVarRef& IVarRef::operator=(IVarRef&& ref) {
    assert(!ref.empty);
    id = std::move(ref.id);
    ref.empty = true;
    empty = false;
    return *this; 
}

IVarRef::IVarRef(const IVarRef& ref):
    IVarRef(ref.id)
{
    assert(!ref.empty);
}

IVarRef& IVarRef::operator=(const IVarRef& ref) {
    assert(empty);
    id = ref.id;
    empty = false;
    cur_worker->inc_ref(*this);
    return *this;
}

} //end namespace taskloaf
