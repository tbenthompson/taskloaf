import logging

import psutil


class Cfg:
    def __init__(self, **kwargs):
        self.hostname = "tcp://127.0.0.1"
        self.n_workers = psutil.cpu_count(logical=False)
        self.affinity = None
        self.base_port = 5754
        self.ports = None
        self.initializer = initializer
        self.connect_to = f"{self.hostname}:{self.base_port}"
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
            "built",
            "run",
        ]:
            setattr(cfg, k, getattr(self, k))
        return cfg


def initializer(name):
    stdout_logging(name)


def stdout_logging(name):
    level = logging.INFO
    tsk_log = logging.getLogger("taskloaf")
    tsk_log.setLevel(level)
    ch = logging.StreamHandler()
    ch.setLevel(level)
    formatter = logging.Formatter(
        f"{name} %(asctime)s [%(levelname)s] %(name)s: %(message)s"
    )
    ch.setFormatter(formatter)
    tsk_log.addHandler(ch)

    # n_cores = psutil.cpu_count(logical=False)

    # if hostname is None:
    #     hostname = defaults.localhost

    # if n_workers is None:
    #     n_workers = n_cores

    # if ports is None:
    #     base_port = defaults.base_port
    #     ports = range(base_port + 1, base_port + n_workers + 1)

    # if connect_to is None:
    #     connect_to = (hostname, ports[0])
