import taskloaf.worker as tsk
from mpi_run import mpirun
from local_run import localrun

def task(i):
    print(str(i))
    if i == 0:
        tsk.shutdown()
    else:
        tsk.submit_task(0, lambda: task(i - 1))

def run(c):
    if c.addr == 0:
        tsk.launch_worker(c)
    else:
        tsk.launch_client(c)
        tsk.submit_task(0, lambda: task(10))

if __name__ == "__main__":
    mpirun(2, run)
