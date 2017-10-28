import asyncio
from taskloaf.worker import submit_task, get_service
from taskloaf.signals import new_id, signal, set_trigger

def complete_task(S, result):
    get_service('data')[S] = result
    signal(S)

class Promise:
    def __init__(self, owner, signal):
        self.owner = owner
        self.signal = signal

    def __await__(self):
        here = get_service('comm').addr
        waiting_futures = get_service('waiting_futures')
        if self.signal not in waiting_futures:
            waiting_futures[self.signal] = []
        waiting_futures[self.signal].append(asyncio.Future())

        def fill_futures(x):
            for fut in get_service('waiting_futures')[self.signal]:
                fut.set_result(x)
        self.then(fill_futures, to = here)
        return waiting_futures[self.signal][-1].__await__()

    def then(self, f, to = None, debug = False):
        if to is None:
            to = self.owner

        if debug:
            print(self.owner, self.signal)
        S = new_id(to)
        submit_task(
            self.owner,
            lambda: set_trigger(self.signal,
                lambda: task(
                    lambda D=get_service('data')[self.signal]: f(D),
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
        to = get_service('comm').addr

    if S is None:
        S = new_id(to)

    def work_wrapper():
        assert(get_service('comm').addr == to)
        result = f()
        if isinstance(result, Promise):
            result.then(lambda x: complete_task(S, x))
        else:
            complete_task(S, result)
    submit_task(to, work_wrapper)
    return Promise(to, S)

def when_all(ps, to = None):
    if to is None:
        to = ps[0].owner

    S = new_id(to)
    n = len(ps)
    for i, p in enumerate(ps):
        def add_to_result(x, i):
            data_store = get_service('data')
            if S not in data_store:
                data_store[S] = [0, [None] * n]
            data_store[S][0] += 1
            data_store[S][1][i] = x
            if data_store[S][0] == n:
                complete_task(S, data_store[S][1])

        p.then(lambda x, i = i: add_to_result(x, i), to = to)
    return Promise(to, S)
