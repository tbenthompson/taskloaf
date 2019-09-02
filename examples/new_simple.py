import cloudpickle

import taskloaf
from taskloaf.zmq import ZMQComm
from taskloaf.mpi import MPIComm
from taskloaf.context import Context
from taskloaf.messenger import JoinMeetMessenger
from taskloaf.executor import Executor
from taskloaf.zmq import zmq_cluster, ZMQClient

# two types of processes: workers and clients...
# "seed worker"
# WorkerContext
# ClientContext
# client connects to worker
# worker connects to other worker

localhost = 'tcp://127.0.0.1'

if __name__ == "__main__":
    # w = mpi_worker()
    n_workers = 3
    base_port = 5755
    with zmq_cluster(
                n_workers,
                localhost,
                [base_port + i for i in range(3)]
            ) as cluster:
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
