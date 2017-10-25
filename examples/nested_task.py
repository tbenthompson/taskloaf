from taskloaf.mpi_comm import MPIComm
import taskloaf.worker

def task(i):
    print(str(i))
    c = taskloaf.worker.comm
    if i == 0:
        taskloaf.worker.shutdown()
    else:
        c.send(c.addr, lambda: task(i - 1))

if __name__ == "__main__":
    c = MPIComm(0)
    if c.addr == 0:
        taskloaf.worker.launch(c)
    else:
        c.send(0, lambda: task(10))
