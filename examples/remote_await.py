import taskloaf as tsk
from taskloaf.promise import task, when_all

async def submit():
    n = await task(lambda: 10, to = 1)
    pr = when_all([task(lambda x = x: x, to = x % 2) for x in range(n)]).then(sum)
    return (await pr)

if __name__ == "__main__":
    print(tsk.cluster(2, submit))
