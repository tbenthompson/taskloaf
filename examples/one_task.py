import time
import multiprocessing
import taskloaf.worker
from taskloaf.local_comm import LocalComm

def die():
    print("DYING")
    taskloaf.worker.run = False

def start(i, qs):
    if i == 0:
        taskloaf.worker.launch(LocalComm(qs, i))
    else:
        c = LocalComm(qs, i)
        c.send(0, lambda: print(i))
        if i == 1:
            time.sleep(1)
            c.send(0, die)

if __name__ == "__main__":
    n = 2
    p = multiprocessing.Pool(n)
    manager = multiprocessing.Manager()
    qs = [manager.Queue() for i in range(n)]

    p.starmap(start, [(i, qs) for i in range(n)])
