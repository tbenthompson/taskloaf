#include "taskloaf.hpp"
#include "taskloaf/print_tree.hpp"
#include "patterns.hpp"

#include <iostream>
#include <cmath>

namespace tsk = taskloaf;

struct Parameters {
    const double left;
    const double right;
    const double sigma;
    const double ao;
    const double coeff;
    const int n_cells;
    const int n_chunks;
    const int n_steps;

    double dx() const {
        return (right - left) / n_cells;
    }

    int n_per_chunk() const {
        return n_cells / n_chunks; 
    }
};

auto initial_conditions(const Parameters& p) {
    std::vector<tsk::Future<std::vector<double>>> temp(p.n_chunks);
    int start_idx = 0;
    int end_idx = p.n_per_chunk();
    for (int i = 0; i < p.n_chunks; i++) {
        temp[i] = tsk::async([=] () {
            std::vector<double> out(end_idx - start_idx);
            for (int j = start_idx; j < end_idx; j++) {
                double x = p.left + p.dx() * j + p.dx() / 2; 
                out[j - start_idx] = p.ao * std::exp(-x * x / (2 * p.sigma * p.sigma));
            }
            return out;
        });

        start_idx += p.n_per_chunk();
        end_idx += p.n_per_chunk();
        end_idx = std::min(p.n_cells, end_idx);
    }
    return temp;
}

auto update(const Parameters& p, int chunk_index, const std::vector<double>& temp,
    double left, double right) 
{
    auto size = temp.size();
    std::vector<double> out = temp;

    if (chunk_index != 0) {
        out.front() += p.coeff * (left - 2 * temp[0] + temp[1]);
    }
    for (size_t cell_idx = 1; cell_idx < temp.size() - 1; cell_idx++) {
        out[cell_idx] += p.coeff * (
            temp[cell_idx - 1] - 2 * temp[cell_idx] + temp[cell_idx + 1]
        );
    }
    if (chunk_index != p.n_chunks - 1) {
        out.back() += p.coeff * (temp[size - 2] - 2 * temp[size - 1] + right);
    }

    return out;
}

auto timestep(const Parameters& p, int step_idx,
    const std::vector<tsk::Future<std::vector<double>>>& temp) 
{
    if (step_idx >= p.n_steps) {
        return temp;
    }

    std::vector<tsk::Future<double>> left_ghosts(p.n_chunks);
    std::vector<tsk::Future<double>> right_ghosts(p.n_chunks);
    for (int i = 0; i < p.n_chunks; i++) {
        left_ghosts[i] = temp[i].then([] (const std::vector<double>& temp) {
            return temp.front();
        });
        right_ghosts[i] = temp[i].then([] (const std::vector<double>& temp) {
            return temp.back();
        });
    }

    std::vector<tsk::Future<std::vector<double>>> new_temp(p.n_chunks);
    for (int i = 0; i < p.n_chunks; i++) {
        tsk::Future<double> left;
        tsk::Future<double> right;
        if (i == 0) {
            left = tsk::ready(0.0);
        } else {
            left = right_ghosts[i - 1];
        }
        if (i == p.n_chunks - 1) {
            right = tsk::ready(0.0);
        } else {
            right = left_ghosts[i + 1];
        }
        new_temp[i] = when_all(temp[i], left, right).then(
            [=] (const std::vector<double>& temp, double left, double right) {
                return update(p, i, temp, left, right);
            }
        );
    }
    return timestep(p, step_idx + 1, new_temp);
}

void diffusion(int n_workers, int n_cells, int time_steps) {
    // TODO: Figure out how to optimize so that the multiplier here gets down to 1.
    // TODO: Work on this "parallelize" chunkification stuff.
    Parameters p{-10, 10, 3.0, 1.0, 0.375, n_cells, n_workers, time_steps};

    tsk::launch(n_workers, [=] () {
        auto temp = initial_conditions(p);

        temp = timestep(p, 0, temp);

        auto tasks = temp.back().then([] (const std::vector<double>& temp) {
            std::cout << temp.front() << std::endl;
            std::cout << temp.back() << std::endl;
            return tsk::shutdown();
        });
        return tasks;
    });
}

int main() {
    diffusion(6, 100000000, 20);
}
