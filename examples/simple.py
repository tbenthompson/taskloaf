import taskloaf.worker as tsk
from taskloaf.mpi import mpirun
from taskloaf.launch import launch

def submit():
    addr = tsk.get_service('comm').addr
    tsk.submit_task(0, lambda: print('hI from proc ' + str(addr)))
    tsk.submit_task(0, tsk.shutdown)

if __name__ == "__main__":
    launch(1, submit)
    launch(1, submit, runner = mpirun)
