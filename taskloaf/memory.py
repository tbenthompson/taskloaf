import os
import attr
import capnp
import asyncio
import taskloaf.message_capnp
import taskloaf.shmem
from taskloaf.dref import DistributedRef, ShmemPtr

#TODO: this is the user facing api!
def put(worker, *, value = None, serialized = None, eager_alloc = 0):
    return worker.memory.put(value = value, serialized = serialized, eager_alloc = eager_alloc)

def get(worker, dref):
    return worker.memory.get_local(dref)

def alloc(worker, nbytes):
    return worker.memory.alloc(nbytes)


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

def is_bytes(v):
    return isinstance(v, bytes) or isinstance(v, memoryview)

# TODO: Separate the responsibilities? Currently MemoryManager has two responsibilities:
# 1) create/tracking reference counted objects
# 2) deciding when to serialize and deserialize objects.
# 3) caching accessed objects
# This feels weird and is hard to program nicely... what if an object requires custom deserialization?
# maybe there's a better approach where memorymanager only deals in already-serialized objects?
# but that has the downside that sometimes objects that don't need to be serialized get serialized.
# hiding the decision of whether or not to serialize from the user sounds ideal...
# on the other hand, that's not a feature that I'm currently using.
#
# an alternate design: this system only deals with the reference counting and we can scrub out all the serialization code.
# then on top of this: a MaybeSerializedDistributedRef can be created that contains a pointer to the value. the serialization rules for such a dref would be identical to the serialization rules for a normal dref. when a maybeserializeddref is serialized, then the underlying object would also be serialized, along with the deserialization function
# or... put could return a dref with the object stored in place requiring no interaction with the memory manager at all. then, once that dref is copied somewhere, the object would be put into the memory manager. this may be my favorite option. "single-owner-optimization"... or put differently, it's a unique_ptr to the dref shared_ptr, but I want both to behave the same way... since this event occurs on the sender side, the serialized blob can be allocated without any extra communication and I can get rid of the ugly eager_alloc hack.

# add a level of indirection in DistributedRef with "impl"?
# also use UniqueRef in some of the internals? essentially when a UniqueRef is serialized, the original reference is destroyed, so that there is always only one reference to the memory. when a reference hits __del__, the original data is remotely freed. when the data is copied to another worker, the original data is immediately freed and the uniqueref is updated to point to the new memory location. this functioning properly relies on a mechanism to prevent copying the reference in the vulnerable period while it is serialized already...
# perhaps there should be some explicitness to the "move" operation for a uniqueref? also disable copying!

# both schemes require a way to deal with custom serialization/deserialization, this is partially represented at the moment by the "needs_deserialize" flag, which is a completely silly hack...
# _taskloaf_serialize and _taskloaf_deserialize monkeypatched into classes that need to have custom serialization?

# should there be a way to pre-allocate certain shared objects on all the machines? essentially the distributed version of a "constant". this could be useful for sharing (de)serialization functions and other functions/data needed by the whole cluster. easier than having some scatter phase that wastes start up time on the task.

# At what level should I specify the serialization/deserialization functions?
# -- system
# -- as method of the class
# -- as methods of the object
# -- as methods separately passed to "put"
# I think allowing access to raw, uncontrolled buffers via "alloc" restricts my options somewhat.
# Actually, I think the status quo is the best option. taskloaf uses cloudpickle, but the user can use their own serialization if they want.

#
# class Transformer:
#     pass

# The value that is "put" into the memory manager should always be the same as
# the value that is "get" from the memory manager, even if the object must be
# serialized and sent across the network.
class MemoryManager:
    def __init__(self, worker, allocator):
        self.local_entries = dict()
        self.serialized_entries = dict()
        self.entries = dict()
        self.worker = worker
        self.worker.protocol.add_msg_type(
            'DECREF',
            serializer = DecRefSerializer,
            handler = lambda w, x: lambda worker: worker.memory.dec_ref_owned(*x)
        )
        self.next_id = 0
        self.allocator = allocator

    def n_entries(self):
        return len(self.entries)

    def get_new_id(self):
        self.next_id += 1
        return self.next_id - 1

    def put(self, *, value = None, serialized = None, dref = None, eager_alloc = 0):
        mem = self._existing_memory(value, serialized, eager_alloc)
        if dref is None:
            put_fnc = self.put_new
        elif dref.owner != self.worker.addr:
            put_fnc = self.put_remote
        else:
            put_fnc = self.put_owned

        return put_fnc(mem, dref)

    def _existing_memory(self, value, serialized, eager_alloc):
        mem = Memory()
        if value is not None:
            mem.set_value(value)
            if eager_alloc >= 1 and is_bytes(value):
                self.serialize_bytes(mem)
            elif eager_alloc >= 2:
                self.serialize_pickle(mem)
        elif serialized is not None:
            mem.set_serialized(ShmemPtr(True, *self.allocator.store(memoryview(serialized))))
        else:
            mem.set_value(None)
        return mem

    def put_new(self, mem, _):
        dref = DistributedRef(self.worker, self.worker.addr)
        return self.put_owned(mem, dref)

    def put_owned(self, mem, dref):
        if mem.has_serialized:
            dref.shmem_ptr = mem.serialized
        self.entries[dref.index()] = Block(memory = mem, ownership = RefCount())
        return dref

    def put_remote(self, mem, dref):
        self.entries[dref.index()] = Block(memory = mem, ownership = None)
        return dref

    def alloc(self, size):
        mem = Memory()
        ptr = ShmemPtr(False, *self.allocator.malloc(size))
        mem.set_value(ptr.dereference(self.allocator.mem))
        mem.set_serialized(ptr)
        return self.put_new(mem, None)

    def get_memory(self, dref):
        return self.entries[dref.index()].memory

    def ensure_deserialized(self, dref):
        blk_mem = self.get_memory(dref)
        if not blk_mem.has_value:
            blk_mem.value = taskloaf.serialize.loads(
                self.worker, blk_mem.serialized.dereference(self.allocator.mem)
            )

    def get_local(self, dref):
        self.ensure_deserialized(dref)
        return self.get_memory(dref).value

    def get_serialized(self, dref):
        blk_mem = self.get_memory(dref)
        if not blk_mem.has_serialized:
            if is_bytes(blk_mem.value):
                self.serialize_bytes(blk_mem)
            else:
                self.serialize_pickle(blk_mem)
        return blk_mem.serialized

    def serialize_bytes(self, blk_mem):
        blk_mem.set_serialized(ShmemPtr(
            False,
            *self.allocator.store(memoryview(blk_mem.value))
        ))
        blk_mem.set_value(blk_mem.serialized.dereference(self.allocator.mem))

    def serialize_pickle(self, blk_mem):
        serialized_mem = memoryview(taskloaf.serialize.dumps(blk_mem.value))
        blk_mem.set_serialized(ShmemPtr(
            True,
            *self.allocator.store(serialized_mem)
        ))

    def available(self, dref):
        return dref.index() in self.entries

    def delete(self, dref):
        del self.entries[dref.index()]

    def dec_ref_owned(self, creator, _id, gen, n_children):
        idx = (creator, _id)
        refcount = self.entries[idx].ownership
        refcount.dec_ref(gen, n_children)
        if not refcount.alive():
            del self.entries[idx]

    def dec_ref(self, creator, _id, gen, n_children, owner):
        if owner != self.worker.addr:
            self.worker.send(
                owner, self.worker.protocol.DECREF,
                (creator, _id, gen, n_children)
            )
        else:
            self.dec_ref_owned(creator, _id, gen, n_children)

class DecRefSerializer:
    @staticmethod
    def serialize(args):
        m = taskloaf.message_capnp.Message.new_message()
        m.init('decRef')
        m.decRef.creator = args[0]
        m.decRef.id = args[1]
        m.decRef.gen = args[2]
        m.decRef.nchildren = args[3]
        return m

    @staticmethod
    def deserialize(w, m):
        return m.decRef.creator, m.decRef.id, m.decRef.gen, m.decRef.nchildren
