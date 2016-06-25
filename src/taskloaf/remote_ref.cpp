#include "remote_ref.hpp"

namespace taskloaf {

thread_local std::unordered_map<void*,bool> delete_tracker;

} //end namespace taskloaf
