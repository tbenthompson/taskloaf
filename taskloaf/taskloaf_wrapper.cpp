#include <pybind11/pybind11.h>

#include "taskloaf.hpp"

namespace py = pybind11;
namespace tl = taskloaf;

static auto pickle_module = py::module::import("dill"); 
static py::object dumps = pickle_module.attr("dumps");
static py::object loads = pickle_module.attr("loads");

namespace cereal {
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
        o = loads.call(py::bytes(dump));
    }
} // end namespace cereal

template <typename F>
auto handle_py_exception(F f) {
    try {
        return f();
    } catch (const pybind11::error_already_set& e) {
        PyErr_Print();
        // Need better python error reporting.
        // PyThreadState *tstate = PyThreadState_GET();
        // if (NULL != tstate && NULL != tstate->frame) {
        //     PyFrameObject *frame = tstate->frame;

        //     printf("Python stack trace:\n");
        //     while (NULL != frame) {
        //         int line = frame->f_lineno;
        //         const char *filename = PyString_AsString(frame->f_code->co_filename);
        //         const char *funcname = PyString_AsString(frame->f_code->co_name);
        //         printf("    %s(%d): %s\n", filename, line, funcname);
        //         frame = frame->f_back;
        //     }
        // }
        throw e;
    }
}

struct PyFuture {
    tl::Future<py::object> fut;

    PyFuture then(const py::object& f) {
        return {tl::when_all(tl::ready(f), fut).then(
            [] (py::object f, py::object val) {
                return handle_py_exception([&] () { return f.call(val); });
            }
        )};
    }

    PyFuture unwrap() {
        return {fut.then([] (py::object val) {
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

PyFuture when_both(const PyFuture& a, const PyFuture& b) {
    return PyFuture{tl::when_all(a.fut, b.fut).then(
        [] (py::object a_o, py::object b_o) {
            return handle_py_exception([&] () {
                auto out = py::object(py::make_tuple(a_o, b_o));
                std::cout << std::string(out.str()) << std::endl;
                return out;
            });
        }
    )};
}

PyFuture ready(const py::object& val) {
    return {tl::ready(val)};
}

PyFuture async(const py::object& f) {
    return {tl::ready(f).then([] (py::object f) {
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

void launch_mpi_wrapper(const py::object& f) {
    tl::launch_mpi([&] () {
        f.call();
    });
}

py::object test_serialization(py::object f) {
    std::stringstream serialized_data;
    cereal::BinaryOutputArchive oarchive(serialized_data);
    oarchive(f);
    py::object f2;
    cereal::BinaryInputArchive iarchive(serialized_data);
    iarchive(f2);
    return f2.call();
}

struct Simple {
    tl::Data d;
};

Simple test_refcounting(py::object a, py::object b) {
    {
        std::cout << "a_rc: " << a.ref_count() << std::endl;
        std::cout << "b_rc: " << b.ref_count() << std::endl;
        auto d1 = tl::make_data(py::object(std::move(a)));
        auto d2 = tl::make_data(py::object(std::move(b)));
        std::cout << "a_rc: " << d1.get_as<py::object>().ref_count() << std::endl;
        std::cout << "b_rc: " << d2.get_as<py::object>().ref_count() << std::endl;
    }
    std::cout << "a_rc: " << a.ref_count() << std::endl;
    std::cout << "b_rc: " << b.ref_count() << std::endl;
    auto vd = std::vector<tl::Data>{tl::make_data(py::make_tuple(a, b))};
    return {tl::make_data(std::move(vd))};
}

void test_refcounting2(py::object f, Simple v) {
    Simple v2 = std::move(v);
    auto obj = v2.d.get_as<std::vector<tl::Data>>()[0].get_as<py::object>();
    std::cout << "a_rc: " << py::object(py::tuple(obj)[0]).ref_count() << std::endl;
    std::cout << "b_rc: " << py::object(py::tuple(obj)[1]).ref_count() << std::endl;
    std::cout << obj.ref_count() << std::endl;
    std::cout << std::string(f.call(obj).str()) << std::endl;
}

PYBIND11_PLUGIN(taskloaf_wrapper) {
    py::module m(
        "taskloaf_wrapper",
        "Python bindings for the Taskloaf distributed task parallelism library"
    );

    m.def("test_serialization", test_serialization);
    m.def("test_refcounting", test_refcounting);
    m.def("test_refcounting2", test_refcounting2);
    py::class_<Simple>(m, "Simple");

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

