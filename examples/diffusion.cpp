#include "taskloaf.hpp"
#include "patterns.hpp"
#include "timing.hpp"

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

auto initial_conditions(const Parameters& p, int start_idx, int end_idx) {
    std::vector<double> out(end_idx - start_idx);
    for (int j = start_idx; j < end_idx; j++) {
        double x = p.left + p.dx() * j + p.dx() / 2; 
        out[j - start_idx] = p.ao * std::exp(-x * x / (2 * p.sigma * p.sigma));
    }
    return out;
}

auto spawn_initial_conditions(const Parameters& p) {
    std::vector<tsk::Future<std::vector<double>>> temp(p.n_chunks);
    int start_idx = 0;
    int end_idx = p.n_per_chunk();
    for (int i = 0; i < p.n_chunks; i++) {
        temp[i] = tsk::async([=] () {
            return initial_conditions(p, start_idx, end_idx);
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
    std::vector<double> out(temp.size());

    if (chunk_index != 0) {
        out.front() = temp[0];
        out.front() += p.coeff * (left - 2 * temp[0] + temp[1]);
    }
    for (size_t cell_idx = 1; cell_idx < temp.size() - 1; cell_idx++) {
        // out[cell_idx] = temp[cell_idx] + p.coeff * (
        //     temp[cell_idx - 1] - 2 * temp[cell_idx] + temp[cell_idx + 1]
        // );
    }
    if (chunk_index != p.n_chunks - 1) {
        out.back() = temp[size - 1];
        out.back() += p.coeff * (temp[size - 2] - 2 * temp[size - 1] + right);
    }

    return out;
}

auto timestep(const Parameters& p, 
    const std::vector<tsk::Future<std::vector<double>>>& temp) 
{
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
        new_temp[i] = when_all(left, right, temp[i]).then(
            [=] (double left, double right, const std::vector<double>& temp) {
                return update(p, i, temp, left, right);
            }
        );
    }
    return std::move(new_temp);
}

tsk::Future<std::vector<double>> wait(auto futures) {
    return reduce(futures, [] (const std::vector<double>&, const std::vector<double>&) {
        return std::vector<double>{};
    });
}

void diffusion(int n_workers, int n_cells, int time_steps, int n_chunks) {
    // TODO: Figure out how to optimize so that the multiplier here gets down to 1.
    Parameters p{-10, 10, 3.0, 1.0, 0.375, n_cells, n_chunks, time_steps};

    tsk::launch(n_workers, [=] () {
        auto temp = spawn_initial_conditions(p);

        for (int i = 0; i < time_steps; i++) {
            temp = timestep(p, temp);
        }

        return wait(temp).then([] (std::vector<double>) { return tsk::shutdown(); });
    });
}

void diffusion_serial(int n_cells, int time_steps) {
    Parameters p{-10, 10, 3.0, 1.0, 0.375, n_cells, 1, time_steps};
    std::vector<double> temp(n_cells);
#pragma omp parallel for schedule(static, 6)
    for (int j = 0; j < n_cells; j++) {
        double x = p.left + p.dx() * j + p.dx() / 2; 
        temp[j] = p.ao * std::exp(-x * x / (2 * p.sigma * p.sigma));
    }

    for (int i = 0; i < time_steps; i++) {
        std::vector<double> subs(n_cells);
        #pragma omp parallel for schedule(static,6)
        for (size_t cell_idx = 1; cell_idx < temp.size() - 1; cell_idx++) {
            subs[cell_idx] = temp[cell_idx] + p.coeff * (
                temp[cell_idx - 1] - 2 * temp[cell_idx] + temp[cell_idx + 1]
            );
        }
        temp = std::move(subs);
    }
    std::cout << temp[0] << std::endl;
}

int main(int, char** argv) {
    int n = std::stoi(std::string(argv[1]));
    int n_steps = std::stoi(std::string(argv[2]));
    int n_workers = std::stoi(std::string(argv[3]));
    int n_chunks = std::stoi(std::string(argv[4]));
    TIC
    (void)argv;
    diffusion_serial(n, n_steps);
    TOC("OMP");
    TIC2
    diffusion(n_workers, n, n_steps, n_chunks);
    TOC("Taskloaf");
}
