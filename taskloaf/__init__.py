from taskloaf.cfg import Cfg
from taskloaf.zmq_cluster import zmq_run
from taskloaf.promise import task, when_all
from taskloaf.object_ref import alloc, put
from taskloaf.profile import Profiler
from taskloaf.timer import Timer

_ctx = None


def set_ctx(ctx):
    global _ctx
    _ctx = ctx


def ctx():
    return _ctx


__all__ = [
    "Cfg",
    "zmq_run",
    "task",
    "when_all",
    "alloc",
    "put",
    "Profiler",
    "Timer",
]
