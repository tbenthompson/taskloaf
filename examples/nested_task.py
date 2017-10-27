import taskloaf.worker as tsk
from taskloaf.launch import launch
from taskloaf.mpi import mpirun

def task(i):
    print(str(i))
    if i == 0:
        tsk.shutdown()
    else:
        tsk.submit_task(0, lambda: task(i - 1))

def submit():
    tsk.submit_task(0, lambda: task(10))

if __name__ == "__main__":
    launch(1, submit, runner = mpirun)
