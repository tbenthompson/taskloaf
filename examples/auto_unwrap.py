import taskloaf as tsk

def task(i):
    print(str(i))
    if i != 0:
        return tsk.task(lambda: task(i - 1))

async def submit():
    await tsk.task(lambda: task(10))

if __name__ == "__main__":
    tsk.cluster(1, submit, runner = tsk.mpirun)
