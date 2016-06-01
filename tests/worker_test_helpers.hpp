#pragma once

#include "taskloaf/default_worker.hpp"
#include "taskloaf/local_comm.hpp"
#include "taskloaf/serializing_comm.hpp"
#include "taskloaf/ring_ref_tracker.hpp"

namespace taskloaf {

RingRefTracker& get_ring_tracker(std::unique_ptr<DefaultWorker>& w) {
    return dynamic_cast<RingRefTracker&>(w->get_ref_tracker());
}

void settle(std::vector<std::unique_ptr<DefaultWorker>>& ws) {
    for (int i = 0; i < 10; i++) {
        for (size_t j = 0; j < ws.size(); j++) {
            cur_worker = ws[j].get();
            ws[j]->recv();
        }
    }
}

std::unique_ptr<DefaultWorker> worker() {
    auto lcq = std::make_shared<LocalCommQueues>(1);
    return std::make_unique<DefaultWorker>(std::make_unique<LocalComm>(lcq, 0));
}

std::vector<std::unique_ptr<DefaultWorker>> workers(int n_workers) {
    auto lcq = std::make_shared<LocalCommQueues>(n_workers);
    std::vector<std::unique_ptr<DefaultWorker>> ws;
    for (int i = 0; i < n_workers; i++) {
        ws.emplace_back(std::make_unique<DefaultWorker>(
            std::make_unique<SerializingComm>(
                std::make_unique<LocalComm>(lcq, i)
            )
        ));
        if (i != 0) {
            ws[i]->introduce(ws[0]->get_addr());
        }
        ws[i]->core_id = i;
        settle(ws);
    }
    return ws;
}

ID id_on_worker(std::unique_ptr<DefaultWorker>& w) {
    auto id = get_ring_tracker(w).get_ring_locs()[0];
    id.secondhalf++;
    return id;
}

}
