from taskloaf.worker import task_loop, shutdown, launch
from taskloaf.test_decorators import mpi_procs
from taskloaf.mpi_comm import MPIComm

def test_shutdown():
    task_loop(shutdown)

def test_closure():
    def f():
        f.x = f.y
        shutdown()
    f.x = 1
    f.y = 2
    task_loop(f)
    assert(f.x == 2)

@mpi_procs(2)
def test_comm_create_task():
    c = MPIComm(0)
    if c.addr == 0:
        launch(c)
    else:
        c.send(0, shutdown)
