from taskloaf.zmq_run import run
from taskloaf.zmq_cluster import zmq_cluster
from taskloaf.zmq_client import zmq_client

__all__ = ["zmq_cluster", "zmq_client", "run"]

# from taskloaf.promise import task, when_all, Promise
# from taskloaf.object_ref import alloc, put#, get
# from taskloaf.local import localrun
# from taskloaf.profile import Profiler
# from taskloaf.timer import Timer
#
_ctx = None


def set_ctx(ctx):
    global _ctx
    _ctx = ctx


def ctx():
    return _ctx


default_base_port = 5754
