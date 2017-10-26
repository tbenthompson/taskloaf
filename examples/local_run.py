import time
import multiprocessing
import taskloaf.worker
from taskloaf.local_comm import LocalComm

def die():
    taskloaf.worker.shutdown()

def run(c):
    if c.addr == 0:
        taskloaf.worker.launch_worker(c)
    else:
        taskloaf.worker.launch_client(c)
        taskloaf.worker.submit_task(0, lambda: print('hI from proc ' + str(c.addr)))
        taskloaf.worker.submit_task(0, die)

def localrun(n, f):
    try:
        p = multiprocessing.Pool(n)
        manager = multiprocessing.Manager()
        qs = [manager.Queue() for i in range(n)]
        fut = p.starmap_async(localstart, [(f, i, qs) for i in range(n)])
        fut.wait()
    finally:
        p.close()
        p.join()

def localstart(f, i, qs):
    try:
        c = LocalComm(qs, i)
        f(c)
    except Exception as e:
        import traceback
        traceback.print_exc()
        raise e

if __name__ == "__main__":
    localrun(2, run)
