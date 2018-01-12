/*cppimport
<%
setup_pybind11(cfg)
cfg['compiler_args'] += [
    '-std=c++11', '-Ofast', '-Wall', '-Werror', '-fopenmp', '-UNDEBUG',
    '-D_hypot=hypot'
]
cfg['linker_args'] += ['-fopenmp']
%>
*/

#include <cmath>
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
