/*cppimport
<%
setup_pybind11(cfg)
%>
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
namespace py = pybind11;

template <typename T>
char* serialize(char* out_buf, const T& v) {
    size_t len = 8;
    char* ptr = reinterpret_cast<char*>(const_cast<T*>(&v));
    memcpy(out_buf, ptr, len);
    return out_buf + len;
}

PYBIND11_MODULE(serialize, m) {
    m.def("f", [] (py::buffer& out_bytes, long a, long b, long c, long d, long e) {
        char* ptr = reinterpret_cast<char*>(out_bytes.request().ptr);
        ptr = serialize(ptr, a);
        ptr = serialize(ptr, b);
        ptr = serialize(ptr, c);
        ptr = serialize(ptr, d);
        ptr = serialize(ptr, e);
    });
}
