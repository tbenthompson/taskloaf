#include <boost/python.hpp>
#include "taskloaf.hpp"

namespace tsk = taskloaf;

namespace cereal {

template<class Archive>
void save(Archive& ar, const boost::python::object& o)
{ 
    auto pickle_module = boost::python::import("cloudpickle"); 
    std::string dump = boost::python::extract<std::string>(
        pickle_module.attr("dumps")(o)
    );
    ar(dump);
}

template<class Archive>
void load(Archive& ar, boost::python::object& o)
{
    std::string dump;
    ar(dump);
    auto pickle_module = boost::python::import("cloudpickle"); 
    o = pickle_module.attr("loads")(dump);
}

} // end namespace cereal

std::string serialize_test(boost::python::object& o) {
    std::stringstream serialized_data;
    cereal::BinaryOutputArchive oarchive(serialized_data);
    oarchive(o);
    return serialized_data.str();
}

boost::python::object deserialize_test(const std::string& dump) {
    std::stringstream serialized_data(dump);
    cereal::BinaryInputArchive iarchive(serialized_data);
    boost::python::object o;
    iarchive(o);
    return o;
}

template <typename F>
auto handle_py_exception(F f) {
    try {
        return f();
    } catch (const boost::python::error_already_set& e) {
        PyErr_Print();
        throw e;
    }
}

struct PyFuture {
    tsk::Future<boost::python::object> fut;

    PyFuture then(boost::python::object f) {
        return {tsk::when_all(tsk::ready(f), fut).then(
            [] (boost::python::object f, boost::python::object val) {
                return handle_py_exception([&] () { return f(val); });
            }
        )};
    }

    PyFuture unwrap() {
        return {fut.then([] (boost::python::object val) {
            return handle_py_exception([&] () {
                PyFuture py_fut = boost::python::extract<PyFuture>(val);
                return py_fut.fut;
            });
        }).unwrap()};
    }
};

PyFuture when_both(PyFuture a, PyFuture b) {
    return {tsk::when_all(a.fut, b.fut).then(
        [] (boost::python::object a, boost::python::object b) {
            return handle_py_exception([&] () {
                boost::python::object out = boost::python::make_tuple(a, b);    
                return out;
            });
        }
    )};
}

PyFuture ready(boost::python::object val) {
    return {tsk::ready(val)};
}

PyFuture async(boost::python::object f) {
    return {tsk::ready(f).then([] (boost::python::object f) {
        return handle_py_exception([&] () { return f(); });
    })};
}

int shutdown() {
    return tsk::shutdown();
}
void launch_local_wrapper(int n_workers, boost::python::object f) {
    tsk::launch_local(n_workers, [&] () {
        return handle_py_exception([&] () { return async(f).unwrap().fut; });
    });
}

void launch_mpi_wrapper(boost::python::object f) {
    tsk::launch_mpi([&] () {
        return handle_py_exception([&] () { return async(f).unwrap().fut; });
    });
}

BOOST_PYTHON_MODULE(taskloaf_wrapper) {
    using namespace boost::python;
    def("launch_local", launch_local_wrapper);
    def("launch_mpi", launch_mpi_wrapper);
    def("ready", ready);
    def("async", async);
    def("shutdown", shutdown);
    def("when_both", when_both);
    def("serialize_test", serialize_test);
    def("deserialize_test", deserialize_test);

    class_<PyFuture>("Future", no_init)
        .def("then", &PyFuture::then)
        .def("unwrap", &PyFuture::unwrap);
}
