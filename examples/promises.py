import taskloaf as tsk
from taskloaf.promise import task, when_all

async def submit(w):
    X = 3.1
    n = 10
    pr = task(w,lambda w: task(w, lambda w: X))
    for i in range(n):
        pr = pr.then(lambda w, x: x + 1)
    pr = when_all([
        pr,
        task(w, lambda w: X, to = 1)
    ]).then(lambda w, x: sum(x))
    print('answer is', await pr)

if __name__ == "__main__":
    tsk.cluster(2, submit)
