from taskloaf import *
import operator
import cloudpickle

def fib_serial(index):
    if index < 3:
        return 1
    else:
        return fib_serial(index - 1) + fib_serial(index - 2)

threshold = 30
def fib(index):
    if index < threshold:
        return async(lambda: fib_serial(index));
    else:
        return when_all(fib(index - 1), fib(index - 2)).then(operator.add)

def done(x):
    print(x)
    return shutdown()

def f():
    return fib(45).then(done)

launch_mpi(f);

# def test_pickling(t):
#     s = serialize_test(t)
#     print(deserialize_test(s))
#
# test_pickling("abc")
# test_pickling(lambda x: 7 * x)
#
# class ABC(object):
#     pass
#
# test_pickling(ABC())
