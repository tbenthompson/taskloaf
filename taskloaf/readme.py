import taskloaf as tl
from operator import add

def fib(index):
    if index < 3:
        return tl.ready(1);
    else:
        return tl.when_all(fib(index - 1), fib(index - 2)).then(add)

def done(x):
    print(x)
    return tl.shutdown()

def main():
    return fib(25).then(done)

tl.launch_mpi(main);
