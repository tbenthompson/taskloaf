#pragma once

#include "address.hpp"

namespace taskloaf {

#define WRITE_STAT(name) \
    ss << "    " << #name << ": " << name << std::endl;

struct logger {
    address worker_addr;
    size_t n_victimized = 0;
    size_t n_attempted_steals = 0;
    size_t n_successful_steals = 0;    
    size_t n_global_fulfills = 0;
    size_t n_tasks_run = 0;

    logger(address addr): worker_addr(addr) {}

    void write_stats(std::ostream& os) {
        std::stringstream ss;
        ss << std::string(80, '=') << std::endl;
        ss << "Stats for worker(" << worker_addr << "): " << std::endl;
        WRITE_STAT(n_victimized);
        WRITE_STAT(n_attempted_steals);
        WRITE_STAT(n_successful_steals);
        WRITE_STAT(n_tasks_run);
        WRITE_STAT(n_global_fulfills);
        os << ss.str();
    }
};

#undef WRITE_STAT

}//end namespace taskloaf
