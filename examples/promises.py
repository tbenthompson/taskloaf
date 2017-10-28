import taskloaf as tsk
from taskloaf.promise import task, when_all

def shutter():
    for i in range(2):
        tsk.submit_task(i, tsk.shutdown)

async def submit():
    X = 3.1
    n = 10
    pr = task(lambda: task(lambda: X))
    for i in range(n):
        pr = pr.then(lambda x: x + 1)
    pr2 = task(lambda: X, to = 1)
    wa = when_all([pr, pr2])
    pr = wa.then(sum).then(print)
    pr.next(shutter)

if __name__ == "__main__":
    tsk.cluster(2, submit)
