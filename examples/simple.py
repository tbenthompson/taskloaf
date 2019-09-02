import taskloaf as tsk
import taskloaf.mpi

async def f():
    print("FFFF")
    return 123
    # await tsk.task(w, lambda w: print('hI'))

if __name__ == "__main__":
    print(tsk.run(f))
    # tsk.run(submit)
    # tsk.cluster(1, submit)
    # tsk.cluster(1, submit, runner = tsk.mpi.mpirun)
