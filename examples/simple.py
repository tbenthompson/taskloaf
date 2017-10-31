import taskloaf as tsk

async def submit():
    await tsk.task(lambda: print('hI'))

if __name__ == "__main__":
    tsk.cluster(1, submit)
    tsk.cluster(1, submit, runner = tsk.mpirun)
