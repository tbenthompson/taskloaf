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

IVarRef::IVarRef(const IVarRef& ref):
    IVarRef(ref.id)
{
    assert(!ref.empty);
}

IVarRef::IVarRef(IVarRef&& ref) {
    assert(!ref.empty);
    id = std::move(ref.id);
    empty = false;
    ref.empty = true;
}

IVarRef& IVarRef::operator=(IVarRef&& ref) {
    assert(!ref.empty);
    ref.empty = true;
    id = std::move(ref.id);
    empty = false;
    return *this; 
}

IVarRef& IVarRef::operator=(const IVarRef& ref) {
    empty = false;
    id = ref.id;
    cur_worker->inc_ref(*this);
    return *this;
}

} //end namespace taskloaf
