import multiprocessing

import cloudpickle
import psutil

from taskloaf.zmq import ZMQComm

test_cfg = dict()

baseport = 5755
def zmqrun(n_workers, f, cfg):
    try:
        ps = []
        for i in range(n_workers):
            port = baseport + i
            friends = [('tcp://127.0.0.1', baseport + j) for j in range(n_workers) if j != i]
            ps.append(multiprocessing.Process(
                target = zmqstart,
                args = (cloudpickle.dumps(f), port, friends)
            ))
        for p in ps:
            p.start()
    finally:
        for p in ps:
            p.join()

def zmqstart(f, port, friends):
    with ZMQComm(port, friends) as c:
        out = cloudpickle.loads(f)(c)

def get_min_port_friend(c):
    min_port = 1e6
    leader = None
    for k, f in c.friends.items():
        if f.port < min_port:
            min_port = f.port
            leader = f
    return f

def wait_for(c, val, get = lambda x: x):
    stop = False
    while not stop:
        msg = c.recv()
        if msg is not None:
            assert(cloudpickle.loads(msg) == get(val))
        stop = True

def test_send_recv():
    n_workers = 4
    def f(c):
        if c.port < min([f.port for f in c.friends.values()]):
            for f in c.friends.values():
                c.send(f._id, cloudpickle.dumps("OHI!"))
            for i in range(n_workers - 1):
                wait_for(c, "OYAY")
        else:
            wait_for(c, "OHI!")
            c.send(get_min_port_friend(c)._id, cloudpickle.dumps("OYAY"))
    zmqrun(n_workers, f, test_cfg)

def data_send(c):
    if c.port == baseport:
        c.send(get_min_port_friend(c)._id, cloudpickle.dumps(123))
    if c.port == baseport + 1:
        wait_for(c, 123)

def fnc_send(c):
    if c.port == baseport:
        x = 13
        c.send(get_min_port_friend(c)._id, cloudpickle.dumps(lambda: x ** 2))
    if c.port == baseport + 1:
        wait_for(c, 169, lambda x: x())

def test_zmq_data_send():
    zmqrun(2, data_send, test_cfg)

def test_zmq_fnc_send():
    zmqrun(2, fnc_send, test_cfg)
