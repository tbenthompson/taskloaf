from taskloaf.mpi_comm import MPIComm
import taskloaf.worker

def task(i):
    print(str(i))
    if i == 0:
        taskloaf.worker.shutdown()
    else:
        taskloaf.worker.submit_task(lambda: task(i - 1))

if __name__ == "__main__":
    c = MPIComm(0)
    if c.addr == 0:
        taskloaf.worker.task_loop(lambda: taskloaf.worker.comm_poll(c))
    else:
        c.send(0, lambda: task(10))
