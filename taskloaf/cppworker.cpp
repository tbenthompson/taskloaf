/*cppimport
<%
setup_pybind11(cfg)
cfg['compiler_args'] += ['-std=c++17']

import mpi4py
cfg['include_dirs'].append(mpi4py.get_include())
%>
*/
#include <pybind11/pybind11.h>
#include <mpi.h>
#include <mpi4py/mpi4py.h>

#include <memory>
#include <iostream>
#include <cassert>

namespace py = pybind11;

struct Worker {

};

int rank(MPI_Comm comm) {
    int out;
    MPI_Comm_rank(comm, &out);
    return out;
}

void CHECKERR(int errcode) {
    if (errcode != 0) {
        throw std::runtime_error("mpi error " + std::to_string(errcode));
    }
}

struct MPIComm {
    py::object pycomm;
    MPI_Comm comm;
    int addr;

    static py::object default_comm;

    MPIComm()
    {
        pycomm = default_comm;
        if (pycomm.is(py::none())) {
            comm = MPI_COMM_WORLD;
        } else {
            comm = *PyMPIComm_Get(pycomm.ptr());
        }
        addr = rank(comm);
    }

    void send(long to_addr, py::buffer mem) {
        auto buf_info = mem.request();
        uint8_t* ptr = reinterpret_cast<uint8_t*>(buf_info.ptr);
        MPI_Request req;
        CHECKERR(MPI_Isend(ptr, buf_info.size, MPI_BYTE, to_addr, 0, comm, &req));
    }

    py::object recv() {
        MPI_Status s;
        int msg_exists;
        CHECKERR(MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &msg_exists, &s));
        if (!msg_exists) {
            return py::none();
        }
        int count;
        CHECKERR(MPI_Get_count(&s, MPI_BYTE, &count));
        auto data = std::make_unique<uint8_t[]>(count);
        CHECKERR(MPI_Recv(
            data.get(), count, MPI_BYTE, s.MPI_SOURCE, MPI_ANY_TAG, comm, &s
        ));
        return py::memoryview(py::bytes(reinterpret_cast<char*>(&data[0]), count));
    }
};

py::object MPIComm::default_comm = py::none();

PYBIND11_MODULE(cppworker, m) {
    if (import_mpi4py() < 0) return;

    py::class_<MPIComm>(m, "MPIComm")
        .def_readonly("addr", &MPIComm::addr)
        // .def_readwrite_static("default_comm", &MPIComm::default_comm)
        .def_readwrite_static("default_comm", &MPIComm::default_comm)
        .def("send", &MPIComm::send)
        .def("recv", &MPIComm::recv)
        .def(py::init<>());

    py::class_<Worker>(m, "Worker")
        .def(py::init<>());
}
