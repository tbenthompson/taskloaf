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
    std::vector<double> out = temp;
    size_t start = 0;
    size_t end = temp.size();
    if (chunk_index == 0) {
        start++; 
    }
    if (chunk_index == p.n_chunks - 1) {
        end--;
    }
    for (size_t cell_idx = start; cell_idx < end; cell_idx++) {

        double to_left = left;
        if(cell_idx > 0) {
            to_left = temp[cell_idx - 1];
        }
        double to_right = right;
        if (cell_idx < temp.size() - 1) { 
            to_right = temp[cell_idx + 1];
        }

        out[cell_idx] += p.coeff * (
            to_left - 2 * temp[cell_idx] + to_right
        );
    }
    return out;
}

auto timestep(const Parameters& p, int step_idx,
    const std::vector<tsk::Future<std::vector<double>>>& temp) 
{
    (void)step_idx;
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
    return new_temp;
}

void diffusion(int n_workers, int n_cells, int time_steps) {
    // TODO: Figure out how to optimize so that the multiplier here gets down to 1.
    // TODO: Work on this "parallelize" chunkification stuff.
    Parameters p{-10, 10, 3.0, 1.0, 0.375, n_cells, n_workers, time_steps};

    tsk::launch(n_workers, [=] () {
        auto temp = initial_conditions(p);

        for (int step = 0; step < time_steps; step++) {
            temp = timestep(p, step, temp);
        }

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
