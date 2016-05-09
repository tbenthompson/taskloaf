#include "ivar.hpp"
#include "worker.hpp"

#include <iostream>
#include <mutex>

namespace taskloaf {

IVarRef::IVarRef():
    empty(true)
{}

IVarRef::IVarRef(ID id):
    IVarRef(std::move(id), initial_ref())
{}

IVarRef::IVarRef(ID id, RefData data):
    id(std::move(id)),
    data(std::move(data)),
    empty(false)
{}

IVarRef::~IVarRef() {
    if (!empty) {
        cur_worker->dec_ref(*this);
    }
}

void move(IVarRef& to, IVarRef&& from) {
    to.id = std::move(from.id);
    to.data = std::move(from.data);
    to.empty = from.empty;
    from.empty = true;
}

void copy(IVarRef& to, const IVarRef& from) {
    to.data = copy_ref(from.data);
    to.id = from.id;
    to.empty = from.empty;
}

IVarRef::IVarRef(IVarRef&& ref) {
    move(*this, std::move(ref));
}

IVarRef& IVarRef::operator=(IVarRef&& ref) {
    move(*this, std::move(ref));
    return *this; 
}

IVarRef::IVarRef(const IVarRef& ref) {
    copy(*this, ref);
}

IVarRef& IVarRef::operator=(const IVarRef& ref) {
    copy(*this, ref);
    return *this;
}

void IVarRef::save(cereal::BinaryOutputArchive& ar) const {
    ar(empty);
    if (!empty) {
        ar(id);
        ar(copy_ref(data));
    }
}

void IVarRef::load(cereal::BinaryInputArchive& ar) {
    ar(empty);
    if (!empty) {
        ar(id);
        ar(data);
    }
}

} //end namespace taskloaf
