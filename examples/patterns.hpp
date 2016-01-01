#pragma once

#include "taskloaf.hpp"

template <typename T>
taskloaf::Future<std::vector<T>> 
concatenate(std::vector<taskloaf::Future<std::vector<T>>>& chunks) {
    return reduce(chunks, [] (const std::vector<T>& in, std::vector<T>& out) {
        out.insert(out.end(), in.begin(), in.end());
        return out;
    });
}

// parallelize, map, aggregate
// 
// template <typename T>
// struct Dataset {
//     std::vector<taskloaf::Future<T>> futures;
// };
// 
// std::vector<size_t> range(size_t a, size_t b) {
//      
// }
// 
// template <typename T>
// Dataset<std::vector<T>> parallelize(std::vector<T> items, int n_partitions) {
//     auto n_per_partition = items.size() / n_partitions; 
//     for (size_t i = 0; i < n_partitions; i++) {
//         auto start_idx = i * n_per_partition;
//         auto end_idx = std::min((i + 1) * n_per_partition, items.size());
//         std::vector<T> chunk(end_idx - start_idx);
//         auto start_it = items.begin() + start_idx;
//         auto end_it = items.begin() + end_idx;
//         std::move(start_it, end_it, chunk.begin());
//         std::erase(start_it, end_it);
//     }
// }

// template <typename T, typename F>
// std::vector<taskloaf::Future<OutT>> map(std::vector<taskloaf::Future<InT>

template <typename T, typename F>
taskloaf::Future<T> reduce(int lower, int upper,
    const std::vector<taskloaf::Future<T>>& vals,
    const F& f) 
{
    if (lower == upper) {
        return vals[lower];
    } else {
        auto middle = (lower + upper) / 2;
        return taskloaf::when_all(
            reduce(lower, middle, vals, f),
            reduce(middle + 1, upper, vals, f)
        ).then(f);
    }
}

template <typename T, typename F>
taskloaf::Future<T> reduce(const std::vector<taskloaf::Future<T>>& vals, const F& f) {
    return reduce(0, vals.size() - 1, vals, f);
}
