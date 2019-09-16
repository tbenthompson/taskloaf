import sys

import taskloaf as tsk

if __name__ == "__main__":

    cfg = tsk.Cfg()
    cfg.n_workers = int(sys.argv[1])
    cfg.base_port = int(sys.argv[2])

    if len(sys.argv) > 3:
        connect_port = int(sys.argv[3])
        cfg.connect_to = (cfg.hostname, connect_port)

    tsk.zmq_run(cfg=cfg)
