from contextlib import ExitStack

import multiprocessing
import cloudpickle
import psutil

import taskloaf
from taskloaf.zmq import ZMQComm
from taskloaf.mpi import MPIComm
from taskloaf.context import Context
from taskloaf.executor import Executor


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
    psutil.Process().cpu_affinity(cpu_affinity)
    with ZMQComm(port, []) as comm:
        cfg = dict()
        with Context(name, comm, cfg) as ctx:
            taskloaf.set_ctx(ctx)
            # TODO: Make Executor into a context manager
            ctx.executor = Executor(ctx.poll_fnc, cfg, ctx.log)
            ctx.executor.start()

class ZMQWorker:
    def __init__(self, name, port, cpu_affinity, hostname = 'tcp://*'):
        self.name = name
        self.port = port
        self.hostname = hostname
        self.p = multiprocessing.Process(
            target = zmq_launcher,
            args = (name, port, hostname, cpu_affinity)
        )

    def __enter__(self):
        self.p.start()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        #TODO: softer exit?
        self.p.terminate()
        self.p.join()

class ZMQClient:
    def __init__(self, port, join_addr, hostname = 'tcp://*'):
        self.name = 1000 #TODO:
        self.port = port
        self.join_addr = join_addr
        self.hostname = hostname

    def __enter__(self):
        self.exit_stack = ExitStack()
        self.comm = self.exit_stack.enter_context(ZMQComm(
            self.port,
            [self.join_addr],
            hostname=self.hostname
        ))
        self.ctx = self.exit_stack.enter_context(Context(
            self.name,
            self.comm,
            dict()
        ))
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.exit_stack.close()

# two types of processes: workers and clients...
# "seed worker"
# WorkerContext
# ClientContext
# client connects to worker
# worker connects to other worker

if __name__ == "__main__":
    # w = mpi_worker()
    with ZMQWorker(0, 5755, [0]) as worker:
        with ZMQClient(5756, ('tcp://127.0.0.1', worker.port)) as client:
            print(client.comm.friends)
            keys = list(client.comm.friends.keys())

            def f():
                global taskloaf_ctx
                print("SENDING")
                taskloaf.ctx().comm.send(
                    list(taskloaf.ctx().comm.friends.keys())[0],
                    cloudpickle.dumps(123)
                )
                taskloaf.ctx().executor.stop = True

            msg = client.ctx.protocol.encode(
                client.name,
                client.ctx.protocol.WORK,
                f
            )
            client.comm.send(keys[0], msg)
            while True:
                msg = client.comm.recv()
                if msg is None:
                    continue
                print("RECEIVED")
                print(msg)
                break
    print("BYE")
