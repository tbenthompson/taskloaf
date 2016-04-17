from taskloaf_wrapper import *
import taskloaf_wrapper
import ctypes

def when_all(*args):
    def make_split_args(f):
        def split_args(x):
            return f(*x)
        return split_args

    def flatten_tuple(t):
        return sum(t, ())

    def when_all_helper(*args):
        stage = []
        for i in range(0, len(args) - 1, 2):
            stage.append(when_both(args[i], args[i + 1]))
        if len(args) % 2 == 1:
            stage.append(args[-1])
        if len(stage) > 1:
            return when_all_helper(*stage).then(flatten_tuple)
        else:
            return stage[0]

    class WhenAllFuture(object):
        def __init__(self, *args):
            self.result = when_all_helper(*args)

        def then(self, f):
            return self.result.then(make_split_args(f))

    return WhenAllFuture(*args)

def launch_mpi(*args, **kwargs):
    ctypes.CDLL('libmpi.so', mode=ctypes.RTLD_GLOBAL)
    taskloaf_wrapper.launch_mpi(*args, **kwargs)
