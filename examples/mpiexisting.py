import taskloaf as tsk
import taskloaf.mpi

async def submit(w):
    return await tsk.task(w, lambda: print('hI'), to = 1)

tsk.cluster(2, submit, runner = tsk.mpi.mpiexisting)
