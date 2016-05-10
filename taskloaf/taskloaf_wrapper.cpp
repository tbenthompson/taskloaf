#include <pybind11/pybind11.h>

#include "taskloaf.hpp"

namespace py = pybind11;
namespace tl = taskloaf;

namespace cereal {
    template<class Archive>
    void save(Archive& ar, const py::object& o)
    { 
        // TODO: Figure out a way to load these once.
        auto pickle_module = py::module::import("dill"); 
        py::object dumps = pickle_module.attr("dumps");
        py::object loads = pickle_module.attr("loads");
        ar(dumps.call(o).cast<std::string>());
    }

    template<class Archive>
    void load(Archive& ar, py::object& o)
    {
        auto pickle_module = py::module::import("dill"); 
        py::object dumps = pickle_module.attr("dumps");
        py::object loads = pickle_module.attr("loads");
        std::string dump;
        ar(dump);
        o = loads.call(py::bytes(dump));
    }
} // end namespace cereal

template <typename F>
auto handle_py_exception(const F& f) {
    try {
        return f();
    } catch (const pybind11::error_already_set& e) {
        // TODO: Need better python error reporting.
        PyErr_Print();
        throw e;
    }
}

struct PyFuture {
    tl::Future<py::object> fut;

    PyFuture then(const py::object& f) {
        return {tl::when_all(tl::ready(f), fut).then(
            [] (py::object& f, py::object& val) {
                return handle_py_exception([&] () { return f.call(val); });
            }
        )};
    }

    PyFuture unwrap() {
        return {fut.then([] (py::object& val) {
            return handle_py_exception([&] () {
                return val.cast<PyFuture*>()->fut;
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

PyFuture when_both(PyFuture& a, PyFuture& b) {
    return PyFuture{tl::when_all(a.fut, b.fut).then(
        [] (py::object& a_o, py::object& b_o) {
            return handle_py_exception([&] () {
                return py::object(py::make_tuple(a_o, b_o));
            });
        }
    )};
}

PyFuture ready(py::object& val) {
    return {tl::ready(val)};
}

PyFuture async(const py::object& f) {
    return {tl::ready(f).then([] (const py::object& f) {
        auto out = handle_py_exception([&] () {
            auto ret = f.call();
            return ret;
        });
        return out;
    })};
}

int shutdown() {
    return tl::shutdown();
}

void launch_local_wrapper(int n_workers, const py::object& f) {
    tl::launch_local(n_workers, [&] () {
        f.call();
    });
}

PYBIND11_PLUGIN(taskloaf_wrapper) {
    py::module m(
        "taskloaf_wrapper",
        "Python bindings for the taskloaf distributed parallel futures library"
    );

    m.def("launch_local", launch_local_wrapper);
    m.def("ready", ready);
    m.def("async", async);
    m.def("shutdown", shutdown);
    m.def("when_both", when_both);

    py::class_<PyFuture>(m, "Future")
        .def("then", &PyFuture::then)
        .def("unwrap", &PyFuture::unwrap)
        .def("__getstate__", PyFuture::getstate)
        .def("__setstate__", PyFuture::setstate);

#ifdef MPI_FOUND
    m.def("launch_mpi", [] (const py::object& f) {
        tl::launch_mpi([&] () {
            f.call();
        });
    });
#endif

    return m.ptr();
}

