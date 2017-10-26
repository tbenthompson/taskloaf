from taskloaf.mpi_comm import MPIComm
import taskloaf.worker
import taskloaf.signals

def task1(i):
    print('task1: ' + str(i))
    if i == 0:
        taskloaf.signals.signal(1)
    else:
        taskloaf.worker.submit_task(lambda: task1(i - 1))

def task2(i):
    print('task2: ' + str(i))
    if i == 10:
        taskloaf.signals.signal(2)
    else:
        taskloaf.worker.submit_task(lambda: task2(i + 1))

if __name__ == "__main__":
    c = MPIComm(0)
    if c.addr == 0:
        taskloaf.worker.launch(c)
    else:
        c.send(0, lambda: task1(10))
        c.send(0, lambda: task2(0))
        def trigger2():
            taskloaf.signals.set_trigger(2, taskloaf.worker.shutdown)
        def trigger_setup():
            print("SET!")
            taskloaf.signals.set_trigger(1, trigger2)
        c.send(0, trigger_setup)

