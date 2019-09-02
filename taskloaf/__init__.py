from taskloaf.zmq import zmq_cluster, zmq_client, run

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
