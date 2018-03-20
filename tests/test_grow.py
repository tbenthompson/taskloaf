import multiprocessing
import psutil
from taskloaf.default_cfg import setup_cfg
from taskloaf.zmq2 import *

def start(cfg = None):
    cfg = setup_cfg(cfg)
    if cfg['cpu_affinity'] is not None:
        psutil.Process().cpu_affinity([cfg['cpu_affinity']])
    c = ZMQComm(cfg)
    quit_msgs = 0
    sent_quit = False
    while True:
        if c.recv() is not None:
            quit_msgs += 1
        if quit_msgs == 2:
            break
        if sent_quit:
            continue
        if len(c.remotes) == 3:
            for k, v in c.remotes.items():
                if not v.initialized:
                    continue
            for k, v in c.remotes.items():
                if k == c.id_:
                    continue
                c.send(k, b'')
            sent_quit = True

def test_zmq_grow():
    n_workers = 3
    ps = []
    main_port = 5755
    next_port = main_port
    for i in range(n_workers):
        cfg = dict()
        cfg['cpu_affinity'] = i
        cfg['zmq_addr'] = 'tcp://127.0.0.1:%s' % (main_port + i)
        if i != 0:
            cfg['zmq_join_addr'] = 'tcp://127.0.0.1:%s' % main_port
        p = multiprocessing.Process(target = start, args = (cfg,))
        p.start()
        ps.append(p)

    for i in range(n_workers):
        ps[i].join()

