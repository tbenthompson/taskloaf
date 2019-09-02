import cloudpickle

import taskloaf
from taskloaf.zmq import ZMQClient

localhost = 'tcp://127.0.0.1'

def main():
    with ZMQClient((localhost, 5700)) as client:
        client.update_cluster_view((localhost, 5755))

        client_name = client.ctx.name
        def f():
            print("SENDING")
            taskloaf.ctx().messenger.send(
                client_name,
                taskloaf.ctx().messenger.protocol.WORK,
                cloudpickle.dumps(123)
            )
            # taskloaf.ctx().executor.stop = True

        names = client.ctx.messenger.endpts.keys()
        print('have endpts!', names)

        client.ctx.messenger.send(
            list(names)[0],
            client.ctx.messenger.protocol.WORK,
            f
        )

        while True:
            import asyncio
            msg = asyncio.get_event_loop().run_until_complete(
                client.ctx.messenger.comm.recv()
            )
            if msg is None:
                continue
            print("RECEIVED")
            print(msg)
            break
    print("BYE")

if __name__ == "__main__":
    main()
