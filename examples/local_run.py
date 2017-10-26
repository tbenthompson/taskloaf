import time
import multiprocessing
import taskloaf.worker
from taskloaf.local_comm import LocalComm

def die():
    taskloaf.worker.shutdown()

def run(c):
    try:
        if c.addr == 0:
            taskloaf.worker.launch(c)
        else:
            c.send(0, lambda: print('hi from proc ' + str(c.addr)))
            c.send(0, die)
    except Exception as e:
        print(traceback.print_exc())
        raise e

def start(i, qs):
    c = LocalComm(qs, i)
    run(c)

if __name__ == "__main__":
    n = 2
    try:
        p = multiprocessing.Pool(n)
        manager = multiprocessing.Manager()
        qs = [manager.Queue() for i in range(n)]
        fut = p.starmap_async(start, [(i, qs) for i in range(n)])
        fut.wait()
    finally:
        p.close()
        p.join()
