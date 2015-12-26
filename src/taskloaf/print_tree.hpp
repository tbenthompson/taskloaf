#pragma once
#include "future.hpp"

#include <ostream>

namespace taskloaf {

inline void print_helper(const FutureNode& data, std::string prefix, std::ostream& out) {
    if (data.type == ThenType) {
        auto then = reinterpret_cast<const Then&>(data);
        out << prefix << "Then" << std::endl;
        print_helper(*then.child, prefix + "  ", out);
    } else if (data.type == UnwrapType) {
        auto unwrap = reinterpret_cast<const Unwrap&>(data);
        out << prefix << "Unwrap" << std::endl;
        print_helper(*unwrap.child, prefix + "  ", out);
    } else if (data.type == AsyncType) {
        out << prefix << "Async" << std::endl;
    } else if (data.type == ReadyType) {
        out << prefix << "Ready" << std::endl;
    } else if (data.type == WhenAllType) {
        auto whenall = reinterpret_cast<const WhenAll&>(data);
        out << prefix << "WhenAll" << std::endl;
        for (auto& c: whenall.children) {
            print_helper(*c, prefix + "  ", out);
        }
    }
}

template <typename T>
void print(Future<T> f, std::ostream& out) {
    print_helper(*f.data, "", out);
}

}
