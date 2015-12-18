#pragma once
#include "future.hpp"

#include <ostream>

namespace taskloaf {

inline void print_helper(FutureData* data, std::string prefix, std::ostream& out) {
    if (Then* then = dynamic_cast<Then*>(data)) {
        out << prefix << "Then" << std::endl;
        print_helper(then->child.get(), prefix + "  ", out);
    } else if (Unwrap* unwrap = dynamic_cast<Unwrap*>(data)) {
        out << prefix << "Unwrap" << std::endl;
        print_helper(unwrap->child.get(), prefix + "  ", out);
    } else if (Async* async = dynamic_cast<Async*>(data)) {
        out << prefix << "Async" << std::endl;
    } else if (Ready* ready = dynamic_cast<Ready*>(data)) {
        out << prefix << "Ready" << std::endl;
    } else if (WhenAll* whenall = dynamic_cast<WhenAll*>(data)) {
        out << prefix << "WhenAll" << std::endl;
        for (auto& c: whenall->children) {
            print_helper(c.get(), prefix + "  ", out);
        }
    }
}

template <typename T>
void print(Future<T> f, std::ostream& out) {
    print_helper(f.data.get(), "", out);
}

}
