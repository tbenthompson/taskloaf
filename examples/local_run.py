import time
import multiprocessing
import taskloaf.worker
from taskloaf.local_comm import LocalComm

def die():
    print("DYING")
    taskloaf.worker.shutdown()

def run(c):
    if c.addr == 0:
        taskloaf.worker.launch(c)
    else:
        c.send(0, lambda: print('hi from proc ' + str(c.addr)))
        if c.addr == 1:
            time.sleep(0.2)
            c.send(0, die)

def start(i, qs):
    c = LocalComm(qs, i)
    run(c)

if __name__ == "__main__":
    n = 2
    p = multiprocessing.Pool(n)
    manager = multiprocessing.Manager()
    qs = [manager.Queue() for i in range(n)]

    p.starmap(start, [(i, qs) for i in range(n)])
