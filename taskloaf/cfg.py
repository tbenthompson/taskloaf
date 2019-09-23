import logging

import psutil


class Cfg:
    def __init__(self, **kwargs):
        self.hostname = "tcp://127.0.0.1"
        self.n_workers = psutil.cpu_count(logical=False)
        self.affinity = None
        self.base_port = 5754
        self.ports = None
        self.initializer = default_setup_worker
        self.connect_to = f"{self.hostname}:{self.base_port}"
        self.log_level = logging.INFO
        self.built = False
        self.run = None
        for k, v in kwargs.items():
            if hasattr(self, k):
                setattr(self, k, v)

    def addr(self, i=0):
        return f"{self.hostname}:{self.ports[i]}"

    def _build(self):
        if self.ports is None:
            self.ports = list(
                range(self.base_port, self.base_port + self.n_workers)
            )

        # TODO: is this right for putting workers on each physical core?
        n_cores = psutil.cpu_count(logical=False)
        if self.affinity is None:
            self.affinity = list(
                map(lambda x: [x % n_cores], range(0, self.n_workers))
            )
        self.built = True

    def get_worker_cfg(self, i):
        assert self.built
        cfg = Cfg()
        cfg.n_workers = 1
        cfg.affinity = [self.affinity[i]]
        cfg.ports = [self.ports[i]]
        for k in [
            "hostname",
            "base_port",
            "initializer",
            "connect_to",
            "log_level",
            "built",
            "run",
        ]:
            setattr(cfg, k, getattr(self, k))
        return cfg


def default_setup_worker(name, cfg):
    stdout_logging(name, log_level=cfg.log_level)


def stdout_logging(name, logger_name="taskloaf", log_level=logging.INFO):
    tsk_log = logging.getLogger(logger_name)
    tsk_log.setLevel(log_level)
    ch = logging.StreamHandler()
    ch.setLevel(log_level)
    formatter = logging.Formatter(
        f"{name} %(asctime)s [%(levelname)s] %(name)s: %(message)s"
    )
    ch.setFormatter(formatter)
    tsk_log.addHandler(ch)
