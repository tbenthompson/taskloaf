from taskloaf.worker import start_worker, start_client
from taskloaf.local import localrun

def launch(n_workers, f, runner = localrun):
    def separate_workers_client(c):
        if c.addr < n_workers:
            start_worker(c)
        else:
            start_client(c)
            f()
    runner(n_workers, separate_workers_client)
