from contextlib import ExitStack

import multiprocessing
import cloudpickle
import psutil

import taskloaf
from taskloaf.zmq import ZMQComm
from taskloaf.mpi import MPIComm
from taskloaf.context import Context
from taskloaf.messenger import JoinMeetMessenger
from taskloaf.executor import Executor


# How do you launch a taskloaf cluster?
# taskloaf.MPIWorker()
# taskloaf.MPIClient()
# taskloaf.ZMQWorker(meet_addr = None)
# taskloaf.ZMQClient(meet_addr = None)


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


def zmq_launcher(addr, cpu_affinity, meet_addr):
    psutil.Process().cpu_affinity(cpu_affinity)
    with ZMQComm(addr) as comm:
        cfg = dict()
        #TODO: addr[1] = port, this is insufficient eventually, use uuid?
        messenger = JoinMeetMessenger(addr[1], comm)
        if meet_addr is not None:
            messenger.meet(meet_addr)
        with Context(messenger, cfg) as ctx:
            taskloaf.set_ctx(ctx)
            # TODO: Make Executor into a context manager
            ctx.executor = Executor(ctx.poll_fnc, cfg, ctx.log)
            ctx.executor.start()

class ZMQWorker:
    def __init__(self, addr, cpu_affinity, meet_addr = None):
        self.addr = addr
        self.p = multiprocessing.Process(
            target = zmq_launcher,
            args = (addr, cpu_affinity, meet_addr)
        )

    def __enter__(self):
        self.p.start()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        #TODO: softer exit?
        self.p.terminate()
        self.p.join()

class ZMQClient:
    def __init__(self, addr, meet_addr):
        self.addr = addr
        self.meet_addr = meet_addr

    def __enter__(self):
        self.exit_stack = ExitStack()
        comm = self.exit_stack.enter_context(ZMQComm(
            self.addr,
            # [self.meet_addr],
        ))
        #TODO: addr[1] = port, this is insufficient eventually, use uuid?
        messenger = JoinMeetMessenger(self.addr[1], comm)
        messenger.meet(self.meet_addr)
        self.ctx = self.exit_stack.enter_context(Context(
            messenger,
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
    n_workers = 3
    localhost = 'tcp://127.0.0.1'
    base_port = 5755
    with ExitStack() as es:

        workers = []
        for i in range(n_workers):
            port = base_port + i
            meet_addr = None
            if i > 0:
                meet_addr = (localhost, base_port)
            workers.append(es.enter_context(ZMQWorker(
                (localhost, port), [i], meet_addr = meet_addr
            )))

        with ZMQClient((localhost, base_port - 1), (localhost, base_port)) as client:
            def f():
                global taskloaf_ctx
                print("SENDING")
                taskloaf.ctx().messenger.send(
                    5754,
                    taskloaf.ctx().messenger.protocol.WORK,
                    cloudpickle.dumps(123)
                )
                taskloaf.ctx().executor.stop = True

            while True:
                client.ctx.poll_fnc()
                if len(client.ctx.messenger.endpts) == 3:
                    break


            names = client.ctx.messenger.endpts.keys()
            print('have endpts!', names)
            import sys
            sys.stdout.flush()

            client.ctx.messenger.send(
                list(names)[0],
                client.ctx.messenger.protocol.WORK,
                f
            )

            while True:
                msg = client.ctx.messenger.comm.recv()
                if msg is None:
                    continue
                print("RECEIVED")
                print(msg)
                break
    print("BYE")
