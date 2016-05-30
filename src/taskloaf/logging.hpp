#pragma once

namespace taskloaf {

struct Log {
    Address worker_addr;
    size_t n_steals = 0;    
    size_t n_global_fulfills = 0;

    Log(Address addr): worker_addr(addr) {}

    void write_stats(std::ostream& os) {
        os << "Stats for worker(" << worker_addr << "): " << std::endl;
        os << "n_steals: " << n_steals << std::endl;
        os << "n_global_fulfills: " << n_global_fulfills << std::endl;
    }
};

}//end namespace taskloaf
