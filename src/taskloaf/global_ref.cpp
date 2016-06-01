#include "global_ref.hpp"
#include "worker.hpp"
#include "ref_tracker.hpp"

#include <iostream>
#include <mutex>

namespace taskloaf {

GlobalRef::GlobalRef():
    empty(true)
{}

GlobalRef::GlobalRef(ID id):
    GlobalRef(std::move(id), initial_ref())
{}

GlobalRef::GlobalRef(ID id, RefData data):
    id(std::move(id)),
    data(std::move(data)),
    empty(false)
{}

GlobalRef::~GlobalRef() {
    if (!empty && cur_worker != nullptr) {
        cur_worker->get_ref_tracker().dec_ref(*this);
    }
}

void move(GlobalRef& to, GlobalRef&& from) {
    to.id = std::move(from.id);
    to.data = std::move(from.data);
    to.empty = from.empty;
    from.empty = true;
}

void copy(GlobalRef& to, const GlobalRef& from) {
    to.data = copy_ref(from.data);
    to.id = from.id;
    to.empty = from.empty;
}

GlobalRef::GlobalRef(GlobalRef&& ref) {
    move(*this, std::move(ref));
}

GlobalRef& GlobalRef::operator=(GlobalRef&& ref) {
    move(*this, std::move(ref));
    return *this; 
}

GlobalRef::GlobalRef(const GlobalRef& ref) {
    copy(*this, ref);
}

GlobalRef& GlobalRef::operator=(const GlobalRef& ref) {
    copy(*this, ref);
    return *this;
}

void GlobalRef::save(cereal::BinaryOutputArchive& ar) const {
    ar(empty);
    if (!empty) {
        ar(id);
        ar(copy_ref(data));
    }
}

void GlobalRef::load(cereal::BinaryInputArchive& ar) {
    ar(empty);
    if (!empty) {
        ar(id);
        ar(data);
    }
}

} //end namespace taskloaf
