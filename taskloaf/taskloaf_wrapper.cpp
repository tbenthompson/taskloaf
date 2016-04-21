#include <pybind11/pybind11.h>

#include "taskloaf.hpp"

namespace py = pybind11;
namespace tsk = taskloaf;

namespace cereal {

static auto pickle_module = py::module::import("dill"); 
static py::object dumps = pickle_module.attr("dumps");
static py::object loads = pickle_module.attr("loads");

template<class Archive>
void save(Archive& ar, const py::object& o)
{ 
    ar(dumps.call(o).cast<std::string>());
}

template<class Archive>
void load(Archive& ar, py::object& o)
{
    std::string dump;
    ar(dump);
    o = loads.call(dump);
}

} // end namespace cereal

template <typename F>
auto handle_py_exception(F f) {
    return f();
    // try {
    //     return f();
    // } catch (const boost::python::error_already_set& e) {
    //     PyErr_Print();
    //     throw e;
    // }
}

struct PyFuture {
    tsk::Future<py::object> fut;

    PyFuture then(py::object f) {
        return {tsk::when_all(tsk::ready(f), fut).then(
            [] (py::object f, py::object val) {
                return handle_py_exception([&] () {
                    return f.call(val);
                });
            }
        )};
    }

    PyFuture unwrap() {
        return {fut.then([] (py::object val) {
            return handle_py_exception([&] () {
                return val.cast<PyFuture>().fut;
            });
        }).unwrap()};
    }

    static py::tuple getstate(const PyFuture& w)
    {
        std::stringstream serialized_data;
        cereal::BinaryOutputArchive oarchive(serialized_data);
        oarchive(w.fut);
        return py::make_tuple(serialized_data.str());
    }

    static void setstate(PyFuture& w, py::tuple state)
    {
        new (&w) PyFuture();
        std::string dump = state[0].cast<std::string>();
        std::stringstream serialized_data(dump);
        cereal::BinaryInputArchive iarchive(serialized_data);
        iarchive(w.fut);
    }
};

PyFuture when_both(PyFuture a, PyFuture b) {
    return {tsk::when_all(a.fut, b.fut).then(
        [] (py::object a, py::object b) {
            return handle_py_exception([&] () {
                py::object out = py::make_tuple(a, b);    
                return out;
            });
        }
    )};
}

PyFuture ready(py::object val) {
    return {tsk::ready(val)};
}

PyFuture async(py::object f) {
    return {tsk::ready(f).then([] (py::object f) {
        return handle_py_exception([&] () {
            return f.call();
        });
    })};
}

int shutdown() {
    return tsk::shutdown();
}

void launch_local_wrapper(int n_workers, py::object f) {
    tsk::launch_local(n_workers, [&] () {
        return handle_py_exception([&] () {
            return async(f).unwrap().fut; 
        });
    });
}

void launch_mpi_wrapper(py::object f) {
    tsk::launch_mpi([&] () {
        return handle_py_exception([&] () { return async(f).unwrap().fut; });
    });
}

PYBIND11_PLUGIN(taskloaf_wrapper) {
    py::module m(
        "taskloaf_wrapper",
        "Python bindings for the Taskloaf distributed task parallelism library"
    );

    m.def("launch_local", launch_local_wrapper);
    m.def("launch_local", launch_local_wrapper);
    m.def("launch_mpi", launch_mpi_wrapper);
    m.def("ready", ready);
    m.def("async", async);
    m.def("shutdown", shutdown);
    m.def("when_both", when_both);

    py::class_<PyFuture>(m, "Future")
        .def("then", &PyFuture::then)
        .def("unwrap", &PyFuture::unwrap)
        .def("__getstate__", PyFuture::getstate)
        .def("__setstate__", PyFuture::setstate);

    return m.ptr();
}

