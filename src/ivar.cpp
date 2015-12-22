#include "ivar.hpp"
#include "run.hpp"

namespace taskloaf {

IVarRef::IVarRef(Address owner, size_t id):
    owner(owner), id(id)
{
    cur_worker->inc_ref(*this);
}

IVarRef::~IVarRef() {
    cur_worker->dec_ref(*this);
}

IVarRef::IVarRef(IVarRef&& ref):
    IVarRef(std::move(ref.owner), std::move(ref.id))
{}

IVarRef::IVarRef(const IVarRef& ref):
    IVarRef(ref.owner, ref.id)
{}

} //end namespace taskloaf
