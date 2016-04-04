#include "task_collection.hpp"
#include "comm.hpp"
#include "protocol.hpp"
#include "address.hpp"

#include <iostream>
#include <cmath>

namespace taskloaf {
    
size_t steal_count(size_t n_remote, size_t n_local) {
    if (n_local >= 2 + n_remote) {
        return (size_t)std::ceil(((double)(n_local - n_remote - 1)) / 4.0);
    } else {
        return 0;
    }
}

TaskCollection::TaskCollection(Comm& comm):
    comm(comm),
    stealing(false)
{
    comm.add_handler(Protocol::Steal, [&] (Data d) {
        auto p = d.get_as<std::pair<Address,size_t>>();
        auto n_steals = steal_count(p.second, tasks.size());
        std::vector<TaskT> steals;
        for (size_t i = 0; i < n_steals; i++) {
            steals.push_back(std::move(tasks.back()));
            tasks.pop_back();
        }
        comm.send(p.first, Msg(Protocol::StealResponse, make_data(std::move(steals))));
    });

    comm.add_handler(Protocol::StealResponse, [&] (Data d) {
        auto stolen_tasks = d.get_as<std::vector<TaskT>>();
        for (auto& t: stolen_tasks) {
            tasks.push_back(std::move(t));
        }
        stealing = false;
    });
}

size_t TaskCollection::size() const {
    return tasks.size();
}

void TaskCollection::add_task(TaskT f) {
    tasks.push_front(std::move(f));
}

TaskT TaskCollection::next() {
    auto t = std::move(tasks.front());
    tasks.pop_front();
    return std::move(t);
}

void TaskCollection::steal() {
    if (stealing) {
        return;
    }
    stealing = true;
    comm.send_random(Msg(Protocol::Steal, make_data(
        std::make_pair(comm.get_addr(), tasks.size())
    )));
}

}//end namespace taskloaf
