import taskloaf as tsk
from taskloaf.promise import task, when_all

async def submit(w):
    for i in range(15):
        X = 3.1
        n = 10
        pr = task(w,lambda w: task(w, lambda w: X))
        for i in range(n):
            pr = pr.then(lambda w, x: x + 1)
        pr2 = task(w, lambda w: X, to = 1)
        wa = when_all([pr, pr2])
        pr = wa.then(lambda w, x: sum(x))
        print(await pr)
        print(w.memory.n_entries())

if __name__ == "__main__":
    print(tsk.cluster(2, submit))#, runner = tsk.mpiexisting))
