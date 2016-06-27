<% 
import os
import pybind11
import mpi4py

cfg['include_dirs'] = [
    pybind11.get_include(),
    pybind11.get_include(True),
    'lib', 'src'
]
cfg['parallel'] = True

def files_in_dir(directory, ext):
    ret = []
    for file in os.listdir(directory):
        file_name, file_ext = os.path.splitext(file)
        if file_ext == '.' + ext:
            ret.append(os.path.join(directory, file))
    return ret

cfg['sources'] = files_in_dir(os.path.join(filedirname, 'src', 'taskloaf'), 'cpp')
cfg['dependencies'] = (
    files_in_dir(os.path.join(filedirname, 'src', 'taskloaf'), 'hpp') +
    files_in_dir(os.path.join(filedirname, 'src'), 'hpp')
)
# IT'S REALLY IMPORTANT THAT OTHER LIBRARIES USE THE SAME SET OF COMPILER 
# ARGUMENTS WHEN LINKING! I just had a bug where -DTASKLOAF_DEBUG was being
# used here, but not in tectosaur. This resulted in "data" structures being
# spliced because they are different sizes depending on whether type info
# is stored or not. Perhaps, there is a separate issue here of maintaining
# ABI compatibility between debug and release versions.
cfg['compiler_args'] = ['-std=c++14', '-O3', '-g', '-Wall', '-Werror']
#cfg['compiler_args'].append('-DMPI_FOUND')
#os.environ['CC'] = mpi4py.get_config()['mpicc']
#os.environ['CXX'] = mpi4py.get_config()['mpicxx']
%>
#include <cereal/types/string.hpp>

#include <pybind11/pybind11.h>

#include "taskloaf.hpp"

namespace py = pybind11;
namespace tl = taskloaf;

namespace pybind11 {
template<class Archive>
void save(Archive& ar, const py::object& o) { 
    // TODO: Figure out a way to load these once.
    auto pickle_module = py::module::import("dill"); 
    py::object dumps = pickle_module.attr("dumps");
    py::object loads = pickle_module.attr("loads");
    ar(dumps(o).cast<const std::string>());
}

template<class Archive>
void load(Archive& ar, py::object& o) {
    auto pickle_module = py::module::import("dill"); 
    py::object dumps = pickle_module.attr("dumps");
    py::object loads = pickle_module.attr("loads");
    std::string dump;
    // ar(dump);
    o = loads(py::bytes(dump));
}
}

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

struct py_future {
    tl::future<py::object> fut;

    py_future then(py::object f) {
        return {fut.then(
            [] (py::object f, py::object& val) {
                return handle_py_exception([&] () { return f(val); });
            }, f
        )};
    }

    py_future unwrap() {
        return {fut.then([] (py::object& val) {
            return handle_py_exception([&] () {
                return val.cast<py_future*>()->fut;
            });
        }).unwrap()};
    }

    py::object get() {
        return fut.get();
    }

    static py::tuple getstate(const py_future& w)
    {
        std::stringstream serialized_data;
        cereal::BinaryOutputArchive oarchive(serialized_data);
        oarchive(w.fut);
        return py::make_tuple(serialized_data.str());
    }

    static void setstate(py_future& w, py::tuple state)
    {
        new (&w) py_future();
        std::string dump = state[0].cast<std::string>();
        std::stringstream serialized_data(dump);
        cereal::BinaryInputArchive iarchive(serialized_data);
        iarchive(w.fut);
    }
};

py_future ready(py::object val) {
    return {tl::ready(std::move(val))};
}


py_future task(const py::object& f) {
    return py_future{tl::task([] (const py::object& f) { return f(); }, f)};
}

PYBIND11_PLUGIN(wrapper) {
    py::module m("wrapper", "Python bindings for the taskloaf library");

    py::class_<tl::context>(m, "Context");
    py::class_<tl::config>(m, "Config")
        .def(py::init<>())
        .def_readwrite("print_stats", &tl::config::print_stats)
        .def_readwrite("interrupt_rate", &tl::config::interrupt_rate);

    m.def("launch_local", tl::launch_local);
    m.def("ready", ready);
    m.def("task", task);

    py::class_<py_future>(m, "Future")
        .def("then", &py_future::then)
        .def("unwrap", &py_future::unwrap)
        .def("get", &py_future::get)
        .def("__getstate__", py_future::getstate)
        .def("__setstate__", py_future::setstate);

#ifdef MPI_FOUND
    m.def("launch_mpi", [] () {
        return tl::launch_mpi();
    });
#endif

    return m.ptr();
}
