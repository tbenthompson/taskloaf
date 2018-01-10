/*cppimport
<%
setup_pybind11(cfg)
cfg['compiler_args'] += [
    '-std=c++11', '-O3', '-Wall', '-Werror', '-fopenmp', '-UNDEBUG',
    '-D_hypot=hypot'
]
cfg['linker_args'] += ['-fopenmp']
%>
*/

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

template <typename T>
using NPArray = pybind11::array_t<T,pybind11::array::c_style>;

template <typename T, typename NPT>
T* as_ptr(NPArray<NPT>& np_arr) {
    return reinterpret_cast<T*>(np_arr.request().ptr);
}
    
template <class I, class F>
void coomv(NPArray<I> rows, NPArray<I> cols,
        NPArray<F> data, NPArray<F> x, NPArray<F> y)
{
    size_t nnz = rows.request().shape[0];
    I* Ai = as_ptr<I>(rows);
    I* Aj = as_ptr<I>(cols);
    F* Ax = as_ptr<F>(data);
    F* Xx = as_ptr<F>(x);
    F* Yx = as_ptr<F>(y);
    for(size_t n = 0; n < nnz; n++){
        Yx[Ai[n]] += Ax[n] * Xx[Aj[n]];
    }
}

#define is_aligned(POINTER, BYTE_COUNT) \
    (((uintptr_t)(const void *)(POINTER)) % (BYTE_COUNT) == 0)
#include <iostream>

template <class I, class F>
void csrmv(NPArray<I> indptr, NPArray<I> indices,
    NPArray<F> data, NPArray<F> x, NPArray<F> y, bool openmp) 
{
    I n_row = indptr.request().shape[0] - 1;
    I* Ap = as_ptr<I>(indptr);
    I* Aj = as_ptr<I>(indices);
    F* Ax = as_ptr<F>(data);
    F* Xx = as_ptr<F>(x);
    F* Yx = as_ptr<F>(y);
    std::cout << "4-byte x aligned: " << is_aligned(Xx, 4) << std::endl;
    std::cout << "4-byte y aligned: " << is_aligned(Yx, 4) << std::endl;
    std::cout << "4-byte indptr aligned: " << is_aligned(Ap, 4) << std::endl;
    std::cout << "4-byte indices aligned: " << is_aligned(Aj, 4) << std::endl;
    std::cout << "4-byte data aligned: " << is_aligned(Ax, 4) << std::endl;

    std::cout << "8-byte x aligned: " << is_aligned(Xx, 8) << std::endl;
    std::cout << "8-byte y aligned: " << is_aligned(Yx, 8) << std::endl;
    std::cout << "8-byte indptr aligned: " << is_aligned(Ap, 8) << std::endl;
    std::cout << "8-byte indices aligned: " << is_aligned(Aj, 8) << std::endl;
    std::cout << "8-byte data aligned: " << is_aligned(Ax, 8) << std::endl;

    std::cout << "16-byte x aligned: " << is_aligned(Xx, 16) << std::endl;
    std::cout << "16-byte y aligned: " << is_aligned(Yx, 16) << std::endl;
    std::cout << "16-byte indptr aligned: " << is_aligned(Ap, 16) << std::endl;
    std::cout << "16-byte indices aligned: " << is_aligned(Aj, 16) << std::endl;
    std::cout << "16-byte data aligned: " << is_aligned(Ax, 16) << std::endl;
#pragma omp parallel for if(openmp)
    for (I i = 0; i < n_row; i++){
        F sum = 0.0;//Yx[i];
        for(I k = Ap[i]; k < Ap[i+1]; k++){
            sum += Ax[k] * Xx[Aj[k]];
        }
        Yx[i] = sum;
    }
}

PYBIND11_MODULE(_sparse, m) {
    m.def("coomv", coomv<int,double>);
    m.def("csrmv", csrmv<int,double>);
}
