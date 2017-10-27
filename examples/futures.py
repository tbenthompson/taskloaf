import taskloaf.worker as tsk
from taskloaf.launch import launch
from taskloaf.signals import set_trigger, new_id, signal
from taskloaf.mpi import mpirun

def complete_task(S, result):
    tsk.get_service('data')[S] = result
    signal(S)

class Promise:
    def __init__(self, owner, signal):
        self.owner = owner
        self.signal = signal

    def then(self, f, to = None, debug = False):
        if to is None:
            to = self.owner

        if debug:
            print(self.owner, self.signal)
        S = new_id(to)
        tsk.submit_task(
            self.owner,
            lambda: set_trigger(self.signal,
                lambda: task(
                    lambda D=tsk.get_service('data')[self.signal]: f(D),
                    to = to,
                    S = S
                )
            )
        )
        return Promise(to, S)

    def next(self, f, *args, **kwargs):
        return self.then(lambda x: f(), *args, **kwargs)

def task(f, to = None, S = None):
    if to is None:
        to = tsk.get_service('comm').addr

    if S is None:
        S = new_id(to)

    def work_wrapper():
        assert(tsk.get_service('comm').addr == to)
        result = f()
        if isinstance(result, Promise):
            result.then(lambda x: complete_task(S, x))
        else:
            complete_task(S, result)
    tsk.submit_task(to, work_wrapper)
    return Promise(to, S)

def when_all(ps, to = None):
    if to is None:
        to = ps[0].owner

    S = new_id(to)
    n = len(ps)
    for i, p in enumerate(ps):
        def add_to_result(x, i):
            data_store = tsk.get_service('data')
            if S not in data_store:
                data_store[S] = [0, [None] * n]
            data_store[S][0] += 1
            data_store[S][1][i] = x
            if data_store[S][0] == n:
                complete_task(S, data_store[S][1])

        p.then(lambda x, i = i: add_to_result(x, i), to = to)
    return Promise(to, S)

#async, then, unwrap, when_all

def shutter():
    for i in range(2):
        tsk.submit_task(i, tsk.shutdown)

def submit():
    X = 3.1
    n = 10
    def submit_locally():
        pr = task(lambda: task(lambda: X))
        for i in range(n):
            pr = pr.then(lambda x: x + 1)
        pr2 = task(lambda: X, to = 1)
        pr = when_all([pr, pr2]).then(sum).then(print)
        pr.next(shutter)
    tsk.submit_task(0, submit_locally)

if __name__ == "__main__":
    launch(2, submit, runner = mpirun)
