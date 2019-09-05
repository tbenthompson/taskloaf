import sys
import time

import taskloaf as tsk

if __name__ == "__main__":

    n_cores = int(sys.argv[1])
    base_port = int(sys.argv[2])

    if len(sys.argv) > 3:
        connect_port = int(sys.argv[3])
        connect_to = (tsk.localhost, connect_port)
    else:
        connect_to = None

    ports = [base_port + i for i in range(n_cores)]

    with tsk.zmq_cluster(n_cores, tsk.localhost, ports, connect_to=connect_to):
        while True:
            time.sleep(0.1)
