import taskloaf as tsk
from taskloaf.promise import task, when_all

async def submit(w):
    n = await task(w, lambda w: 10, to = 1)
    pr = when_all([task(w,lambda w,x = x: x, to = x % 2) for x in range(n)]).then(lambda w,x: sum(x))
    return (await pr)

if __name__ == "__main__":
    assert(tsk.cluster(2, submit) == 45)
