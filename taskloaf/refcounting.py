# from taskloaf.allocator import ShmemAllocator, BlockManager
# from taskloaf.shmem import page4kb
import taskloaf.serialize
import copy
import pickle
import attr

def put(worker, obj):
    pass

def alloc(worker, nbytes):
    pass

"""
Why is non-causality a problem for a incref/decref system?
v=1 refcount stored on node #3
ref A gets copied to ref B on node #1
ref B gets sent to node #2
ref A gets deleted on node #1
ref B gets used on node #2
ref B gets deleted on node #2
ref B deletion decref on node #3 --> v=0, object deleted!
ref B creation incref on node #3 --> error, object already deleted!
ref A deletion decref on node #3
This particular example isn't too bad since everyone is already done using
the object, but worse examples are possible, just harder to write down.

So, instead we use generational reference counting:
https://dl.acm.org/citation.cfm?id=74846
"""
class RefCount:
    def __init__(self):
        self.gen_counts = [1]

    def dec_ref(self, gen, n_children):
        n_gens = len(self.gen_counts)
        if n_gens < gen + 2:
            self.gen_counts.extend([0] * (gen + 2 - n_gens))
        self.gen_counts[gen] -= 1
        self.gen_counts[gen + 1] += n_children

    def alive(self):
        for c in self.gen_counts:
            if c != 0:
                return True
        return False

class RefManager:
    @attr.s
    class Entry:
        refcount = attr.ib()
        ref = attr.ib()

    def __init__(self, worker):
        self.worker = worker
        self.entries = dict()

    def dec_ref_owned(self, _id, gen, n_children):
        refcount = self.entries[_id].ownership
        refcount.dec_ref(gen, n_children)
        if not refcount.alive():
            del self.entries[_id]

    def dec_ref(self, _id, gen, n_children, owner):
        if owner != self.worker.addr:
            self.worker.send(
                owner, self.worker.protocol.DECREF,
                (_id, gen, n_children)
            )
        else:
            self.dec_ref_owned(_id, gen, n_children)

    def get_ref(self, _id):
        if _id not in self.entries:
            return None
        else:
            return self.entries[_id].ref

    def store_ref(self, _id, ref):
        self.entries[_id] = RefManager.Entry(
            refcount = RefCount(), ref = ref
        )

"""
This class barely needs to do anything because python already uses reference
counting for GC. It just needs to make sure that once it is serialized, all
serialized versions with the same id point to the same serialized chunk of
memory. This is based on the observation that in order to access a chunk of
memory from another worker, the reference first has to arrive at that other
worker and thus serialization of the reference can be used as a trigger for
serialization of the underlying object.
"""
class Ref:
    def __init__(self, worker, obj):
        self.worker = worker
        self._id = self.worker.get_new_id()
        self.obj = obj

    def get(self):
        return self.obj

    def __getstate__(self):
        raise Exception()

    def convert(self):
        ref = self.worker.ref_manager.get_ref(self._id)
        if ref is None:
            ref = self._new_ref()
        return ref

    def _new_ref(self):
        deserialize, serialized_obj = serialize_if_needed(self.obj)
        nbytes = len(serialized_obj)
        ptr = self.worker.allocator.malloc(nbytes)
        ptr.deref()[:] = serialized_obj
        ref = GCRef(self.worker, self._id, ptr, deserialize)
        self.worker.ref_manager.store_ref(self._id, ref)
        self.worker.object_cache[(self.worker.addr, self._id)] = self.obj
        return ref

def is_bytes(v):
    return isinstance(v, bytes) or isinstance(v, memoryview)

def serialize_if_needed(obj):
    if is_bytes(obj):
        return False, obj
    else:
        return True, taskloaf.serialize.dumps(obj)

"""
It seems like we're recording two indexes to the data:
    -- the ptr itself
    -- the (owner, _id) pair
This isn't strictly necessary, but has some advantages.
"""
class GCRef:
    def __init__(self, worker, _id, ptr, deserialize):
        self.worker = worker
        self.owner = worker.addr
        self._id = _id
        self.gen = 0
        self.n_children = 0
        self.ptr = ptr
        self.deserialize = deserialize

    def get(self):
        key = (self.owner, self._id)
        if key in self.worker.object_cache:
            return self.worker.object_cache[key]
        else:
            # TODO: should remote_get here.
            return self._get_from_buf()

    def _get_from_buf(self):
        buf = self.ptr.deref()
        if self.deserialize:
            return taskloaf.serialize.loads(self.worker, buf)
        else:
            return buf

    def __getstate__(self):
        raise Exception()

#TODO: could be combined with the RemoteShmemRepo?
# class ObjectCache:
#     def __init__(self):
#         self.objs = dict()
#
#     def put(self, key, value):
#         self.objs[key] = value
#
#     def get(self, _id):
#         if _id in self.objs:
#             return (True, self.objs[_id])
#         else:
#             return (False, None)
