import cloudpickle

import taskloaf as tsk
from taskloaf.zmq import ZMQClient

localhost = "tcp://127.0.0.1"


def main():
    with tsk.zmq_client((localhost, 5755)) as c:

        async def f():
            print("SENDING")
            return 123

        print(c.wait(c.submit(f)))


if __name__ == "__main__":
    main()
