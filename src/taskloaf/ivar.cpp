#include "ivar.hpp"
#include "run.hpp"
#include "worker.hpp"

namespace taskloaf {

IVarRef::IVarRef(Address owner, ID id):
    owner(std::move(owner)), id(id)
{
    cur_worker->inc_ref(*this);
}

IVarRef::~IVarRef() {
    if (!owner.hostname.empty()) {
        cur_worker->dec_ref(*this);
    }
}

IVarRef::IVarRef(IVarRef&& ref) {
    owner = std::move(ref.owner);
    id = std::move(ref.id);
}

IVarRef::IVarRef(const IVarRef& ref):
    IVarRef(ref.owner, ref.id)
{}

} //end namespace taskloaf
