import multiprocessing
import cloudpickle
import psutil
from taskloaf.zmq import ZMQComm
from taskloaf.mpi import MPIComm
from taskloaf.context import Context


# How do you launch a taskloaf cluster?
# taskloaf.MPIWorker()
# taskloaf.MPIClient()
# taskloaf.ZMQWorker(join_addr = None)
# taskloaf.ZMQClient(join_addr = None)


def f(comm):
    print("HI", comm)

def connect(cluster):
    pass

def mpi_worker():
    orig_sys_except = sys.excepthook
    if die_on_exception:
        def die(*args, **kwargs):
            orig_sys_except(*args, **kwargs)
            sys.stderr.flush()
            MPI.COMM_WORLD.Abort()
        sys.excepthook = die

    n_mpi_procs = MPI.COMM_WORLD.Get_size()
    if n_workers > n_mpi_procs:
        raise Exception(
            'There are only %s MPI processes but %s were requested' % (n_mpi_procs, n_workers)
        )
    c = MPIComm()
    out = f(c) if c.name < n_workers else None
    sys.excepthook = orig_sys_except
    return out


def zmq_launcher(name, port, hostname, cpu_affinity):
    psutil.Process().cpu_affinity([cpu_affinity])
    hosts = dict()
    hosts[name] = (hostname, port)
    with ZMQComm(name, hosts) as comm:
        with Context(comm, dict()) as ctx:
            ctx.executor.start()

class ZMQWorker:
    def __init__(self, name, port, cpu_affinity, hostname = 'tcp://*'):
        self.p = multiprocessing.Process(
            target = zmq_launcher,
            args = (name, port, hostname, cpu_affinity)
        )

    def __enter__(self):
        self.p.start()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.p.join()


# two types of processes: workers and clients...
# "seed worker"
# WorkerContext
# ClientContext
# client connects to worker
# worker connects to other worker

class ZMQClient:
    def __init__(self):
        pass

if __name__ == "__main__":
    # w = mpi_worker()
    with ZMQWorker(0, 5755, 0):
        pass
