from taskloaf_wrapper import *

# Calculate a fibonacci number recursively in serial.
def fib_serial(index):
    if index < 3:
        return 1
    else:
        return fib_serial(index - 1) + fib_serial(index - 2)

def done(x):
    print(x)
    return shutdown()

def main():
    async(lambda: fib_serial(1)).then(done)

launch_mpi(main)


# def p(x):
#     print(x)
#     return 5
#
# def get_v():
#     v = test_refcounting(43567, 41332)
#     return v
# v = get_v()
#
# for i in range(10):
#     test_refcounting2(p, v)
#
