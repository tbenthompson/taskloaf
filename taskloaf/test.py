"""
Computing the Fibonacci sequence recursively and in parallel seems like a silly
thing to do, but it's a decent example for showing off a task parallelism tool.
The task graph forms a tree, so there's non-trivial task scheduling. Also, it's
incredibly simple and everyone understands the problem being solved!
"""

# Import taskloaf!
import taskloaf as tl

# Calculate a fibonacci number recursively in serial.
def fib_serial(index):
    if index < 3:
        return 1
    else:
        return fib_serial(index - 1) + fib_serial(index - 2)

# Calculate a fibonacci number recursively, in parallel, using taskloaf
def fib(index):
    # If the problem is too small, just do it in serial...
    if index < 25:
        # async(f) tells taskloaf to run the parameterless function f
        # at some point in the future. The return value from async is a
        # future. You can think of a future as a box around a value that
        # will eventually be computed, but has not necessarily been computed
        # yet. Futures are eventually "filled" after the task has run.
        return tl.async(lambda: fib_serial(index));
    else:
        # "when_all" tells taskloaf to return a new future that will be filled
        # with the values from the futures that are passed as arguments
        # after all those futures have been filled.
        # "when_all" is short for "when all of these have been computed"
        return tl.when_all(
            fib(index - 1), fib(index - 2)
        # "future.then" tells taskloaf to run the provided function on the
        # values inside the future after it has been filled
        ).then(lambda x, y: x + y)

# Finish up...
def done(x):
    print(x)
    # shutdown tells taskloaf that it should end the program and not expect any
    # more tasks to run.
    return tl.shutdown()

def main():
    import sys
    n = int(sys.argv[1])
    # Calculate the specified fibonacci number, then print it and shutdown
    return fib(n).then(done)

# Launch an MPI taskloaf application with main as the first task.
tl.launch_mpi(main);
