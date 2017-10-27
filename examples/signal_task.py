from taskloaf.worker import submit_task, shutdown, get_service
from taskloaf.signals import signal, set_trigger
from taskloaf.launch import launch

from uuid import uuid1 as uuid

x = 3

def submit_signalled_task(to, f):
    S = uuid(to)
    def work_wrapper():
        f()
        signal(S)
    submit_task(to, work_wrapper)
    return (to, S)

def when_all(sigs, f):
    if len(sigs) == 1:
        def trigger():
            f()
    else:
        def trigger():
            when_all(sigs[1:], f)
    submit_task(sigs[0][0], lambda: set_trigger(sigs[0][1], trigger))

def task(i):
    print('task: ' + str(i) + ' on ' + str(get_service('comm').addr))

def shutter():
    print('shutter running on ' + str(get_service('comm').addr))
    for i in range(2):
        submit_task(i, shutdown)

def submit():
    n = 5
    sigs = [submit_signalled_task(i < (n / 2), lambda: task(i)) for i in range(n)]
    when_all(sigs, shutter)

if __name__ == "__main__":
    launch(2, submit)
