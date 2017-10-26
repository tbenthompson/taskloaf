from taskloaf.mpi_comm import MPIComm
from taskloaf.worker import submit_task, shutdown, launch, get_service
from taskloaf.signals import signal, set_trigger

def task1(i):
    print('task1: ' + str(i))
    if i == 0:
        signal(1)
    else:
        submit_task(lambda: task1(i - 1))

def task2(i):
    print('task2: ' + str(i))
    if i == 10:
        signal(2)
    else:
        submit_task(lambda: task2(i + 1))

if __name__ == "__main__":
    c = MPIComm(0)
    if c.addr < 2:
        launch(c)
    else:
        c.send(0, lambda: task1(10))
        c.send(1, lambda: task2(0))

        def shutter():
            c = get_service('comm')
            print('shutter running on ' + str(c.addr))
            c.send(0, shutdown)
            shutdown()

        def trigger2():
            c = get_service('comm')
            print('trigger2 running on ' + str(c.addr))
            c.send(1,
                lambda: set_trigger(2, shutter)
            )

        def trigger_setup():
            print("SET!")
            set_trigger(1, trigger2)
        c.send(0, trigger_setup)

