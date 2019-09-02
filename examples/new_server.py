import time

import taskloaf
if __name__ == "__main__":
    with taskloaf.zmq_cluster(3, 'tcp://127.0.0.1', [5755, 5756, 5757]):
        while True:
            time.sleep(0.1)
