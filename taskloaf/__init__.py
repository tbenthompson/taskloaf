from taskloaf.zmq_cluster import zmq_cluster
from taskloaf.zmq_run import run
from taskloaf.promise import task, when_all

_ctx = None


def set_ctx(ctx):
    global _ctx
    _ctx = ctx


def ctx():
    return _ctx


__all__ = ["zmq_cluster", "zmq_client", "run", "task", "when_all"]
