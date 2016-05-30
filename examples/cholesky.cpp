#include "taskloaf.hpp"
#include "taskloaf/timing.hpp"

#include <random>
#include <chrono>
#include <iostream>

#define MATIDX(r,c,n) (c) * (n) + (r)

namespace tsk = taskloaf;

typedef std::vector<double> Matrix;
typedef std::vector<Matrix> Blocks;
typedef std::vector<tsk::Future<Matrix>> FutureList;
typedef std::vector<FutureList> FutureListList;

Matrix random_spd_matrix(size_t n) 
{
    Matrix A(n * n);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-1.0, 1.0);
    for (size_t i = 0; i < n; i++) {
        for (size_t j = i; j < n; j++) {
            A[i * n + j] = dis(gen);
            A[j * n + i] = A[i * n + j];
        }
        A[i * n + i] += n;
    }
    return A;
}

Blocks to_blocks(const Matrix& A, size_t n_blocks)
{
    auto n = std::sqrt(A.size());
    auto n_per_block = n / n_blocks;
    Blocks block_A(n_blocks * n_blocks, Matrix(n_per_block * n_per_block));
    for (size_t b_r = 0; b_r < n_blocks; b_r++)
    {
        for (size_t b_c = 0; b_c < n_blocks; b_c++)
        {
            for (size_t i = 0; i < n_per_block; i++)
            {
                for (size_t j = 0; j < n_per_block; j++)
                {
                    auto full_row_idx = b_r * n_per_block + i;
                    auto full_col_idx = b_c * n_per_block + j;
                    auto block_idx = MATIDX(b_r,b_c,n_blocks);
                    auto entry_idx = MATIDX(i,j,n_per_block);
                    auto orig_idx = MATIDX(full_row_idx, full_col_idx, n);
                    block_A[block_idx][entry_idx] = A[orig_idx];
                }
            }
        }
    }

    return block_A;
}

double check_error(const Matrix& correct, const Matrix& estimate)
{
    auto n = std::sqrt(correct.size());
    double sum = 0;
    for (size_t i = 0; i < n; i++)
    {
        for (size_t j = 0; j < n; j++)
        {
            if (j > i) {
                continue; 
            }
            auto idx = MATIDX(i,j,n);
            auto diff = correct[idx] - estimate[idx];
            sum += diff * diff;
        }
    }
    auto error = std::sqrt(sum) / (n * n);
    return error;
}

extern "C" void dpotrf_(char* UPLO, int* N, double* A, int* LDA, int* INFO);

Matrix dpotrf_task(Matrix& matrix)
{
    // TIC
    char uplo = 'L';
    int info;
    int n = static_cast<int>(std::sqrt(matrix.size()));
    std::cout << n << std::endl;
    dpotrf_(&uplo, &n, matrix.data(), &n, &info);
    // TOC("POTRF");
    return std::move(matrix);
}

extern "C" void dtrsm_(char* SIDE, char* UPLO, char* TRANSA, char* DIAG,
    int* M, int* N, double* ALPHA, double* A, int* LDA, double* B, int* LDB);

Matrix dtrsm_task(Matrix& L_block, Matrix& A_block) 
{
    // TIC
    int n = static_cast<int>(std::sqrt(L_block.size()));
    char side = 'R';
    char uplo = 'L';
    char trans = 'T';
    char unit = 'N';
    double alpha = 1.0;
    dtrsm_(
        &side, &uplo, &trans, &unit, &n, &n, &alpha, L_block.data(),
        &n, A_block.data(), &n
    );
    // TOC("TRSM");
    return std::move(A_block);
}

extern "C" void dsyrk_(char* UPLO, char* TRANS, int* N, int* K, double* ALPHA,
    double* A, int* LDA, double* BETA, double* C, int* LDC);

Matrix dsyrk_task(Matrix& L_block1, Matrix& A_block) 
{
    // TIC
    int n = static_cast<int>(std::sqrt(L_block1.size()));
    char uplo = 'L';
    char trans = 'N';
    double alpha = -1.0;
    double beta = 1.0;
    dsyrk_(&uplo, &trans, &n, &n, &alpha,
        L_block1.data(), &n, &beta, A_block.data(), &n
    );
    // TOC("SYRK");
    return std::move(A_block);
}

extern "C" void dgemm_(char* TRANSA, char* TRANSB, int* M, int* N, int* K,
    double* ALPHA, double* A, int* LDA, double* B, int* LDB, double* BETA, 
    double* C, int* LDC);

Matrix dgemm_task(Matrix& L_block1, Matrix& L_block2, Matrix& A_block) 
{
    // TIC;
    int n = static_cast<int>(std::sqrt(L_block1.size()));
    char transa = 'N';
    char transb = 'T';
    double alpha = -1.0;
    double beta = 1.0;
    dgemm_(
        &transa, &transb, &n, &n, &n, &alpha, L_block1.data(), &n,
        L_block2.data(), &n, &beta, A_block.data(), &n
    );
    // TOC("GEMM");
    return std::move(A_block);
}

FutureList submit_input_data(Blocks& blocks)
{
    FutureList out;
    for (size_t i = 0; i < blocks.size(); i++) {
        out.push_back(tsk::ready(std::move(blocks[i])));
    }
    return out;
}

FutureList cholesky_plan(FutureList inputs)
{
    auto n_b = std::sqrt(inputs.size());

    auto upper_left = inputs[0].then(dpotrf_task);
    if (n_b == 1) {
        return FutureList{upper_left};
    }

    std::cout << n_b << std::endl;
    FutureList lower_left(n_b - 1);
    for (size_t i = 1; i < n_b; i++) {
        lower_left[i - 1] = when_all(
            upper_left, inputs[MATIDX(i, 0, n_b)]
        ).then(dtrsm_task);
    }

    FutureList lower_right(std::pow(n_b - 1, 2));
    for (size_t i = 1; i < n_b; i++) {
        for (size_t j = 1; j <= i; j++) {
            auto old_idx = MATIDX(i, j, n_b);
            auto new_idx = MATIDX(i - 1, j - 1, n_b - 1);
            if (i == j) {
                lower_right[new_idx] = when_all(
                    lower_left[i - 1], inputs[old_idx]
                ).then(dsyrk_task);
            } else {
                lower_right[new_idx] = when_all(
                    lower_left[i - 1], lower_left[j - 1], inputs[old_idx]
                ).then(dgemm_task);
            }
        }
    }

    auto lower_right_inv = cholesky_plan(lower_right);
    
    FutureList full_out(n_b * n_b);

    full_out[0] = std::move(upper_left);
    for (size_t i = 1; i < n_b; i++) {
        full_out[i] = std::move(lower_left[i - 1]);
    }

    for (size_t i = 1; i < n_b; i++) {
        for (size_t j = 1; j <= i; j++) {
            auto in_idx = MATIDX(i - 1, j - 1, n_b - 1);
            full_out[MATIDX(i, j, n_b)] = std::move(lower_right_inv[in_idx]);
        }
    }

    return full_out;
}

void run(int n, int n_blocks, int n_workers, bool run_blas) {
    auto A = random_spd_matrix(n);

    auto correct = A;

    if (run_blas) {
        TIC
        correct = dpotrf_task(correct);
        TOC("BLAS");
    }

    auto block_A = to_blocks(A, n_blocks);
    auto block_correct = to_blocks(correct, n_blocks);

    TIC
    tsk::launch_local(n_workers, [&] () {
        auto input_futures = submit_input_data(block_A);
        auto result_futures = cholesky_plan(std::move(input_futures));
        if (run_blas) {
            auto correct_futures = submit_input_data(block_correct);
            auto total_error = tsk::ready<double>(0.0);
            for (int i = 0; i < n_blocks; i++) {
                for (int j = 0; j < n_blocks; j++) {
                    if (j > i) {
                        continue;
                    }
                    auto idx = MATIDX(i, j, n_blocks);
                    auto block_error = when_all(
                        correct_futures[idx], result_futures[idx]
                    ).then(check_error);
                    total_error = when_all(
                        block_error, total_error
                    ).then(std::plus<double>());
                }
            }
            return total_error.then([] (double x) {
                std::cout << x << std::endl;
                return tsk::shutdown();
            });
        } else {
            return result_futures.back().then([] (Matrix a) {
                (void)a; return tsk::shutdown(); 
            });
        }
    });
    TOC("Taskloaf");
}

int main(int argc, char** argv) {
    (void)argc;
    int n = std::stoi(std::string(argv[1]));
    size_t n_blocks = std::stoi(std::string(argv[2]));
    int n_workers = std::stoi(std::string(argv[3]));
    bool run_blas = std::string(argv[4]) == "true";
    run(n, n_blocks, n_workers, run_blas);
}
