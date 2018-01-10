import os
import attr
import capnp
import asyncio
import taskloaf.message_capnp
import taskloaf.shmem
from taskloaf.dref import DistributedRef, ShmemPtr

#TODO: this is the user facing api!
def put(worker, *, value = None, serialized = None):
    return worker.memory.put(value = value, serialized = serialized)

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

@attr.s
class Memory:
    has_value = attr.ib(default = False)
    value = attr.ib(default = None)
    has_serialized = attr.ib(default = False)
    serialized = attr.ib(default = None)

    def set_value(self, v):
        self.has_value = True
        self.value = v

    def set_serialized(self, s):
        self.has_serialized = True
        self.serialized = s

@attr.s
class Block:
    memory = attr.ib()
    ownership = attr.ib()

# The value that is "put" into the memory manager should always be the same as
# the value that is "get" from the memory manager, even if the object must be
# serialized and sent across the network.
class MemoryManager:
    def __init__(self, worker, alloc):
        self.blocks = dict()
        self.worker = worker
        self.worker.protocol.add_msg_type(
            'DECREF',
            serializer = DecRefSerializer,
            handler = lambda w, x: lambda worker: worker.memory.dec_ref_owned(*x)
        )
        self.next_id = 0
        self.alloc = alloc

    def n_entries(self):
        return len(self.blocks)

    def get_new_id(self):
        self.next_id += 1
        return self.next_id - 1

    def put(self, *, value = None, serialized = None, dref = None):
        putter = self.put_owned
        if dref is None:
            dref = DistributedRef(self.worker, self.worker.addr)
        elif dref.owner != self.worker.addr:
            putter = self.put_remote

        putter(value, serialized, dref)
        return dref

    def make_memory(self, value, serialized):
        mem = Memory()
        if value is not None:
            mem.set_value(value)
            if is_bytes(value):
                mem.set_serialized(ShmemPtr(
                    False,
                    *self.alloc.store(memoryview(value))
                ))
        elif serialized is not None:
            mem.set_serialized(ShmemPtr(True, *self.alloc.store(memoryview(serialized))))
        else:
            mem.set_value(None)
        return mem

    def put_owned(self, value, serialized, dref):
        mem = self.make_memory(value, serialized)
        if mem.has_serialized:
            dref.shmem_ptr = mem.serialized
        self.blocks[dref.index()] = Block(memory = mem, ownership = RefCount())

    def put_remote(self, value, serialized, dref):
        mem = self.make_memory(value, serialized)
        self.blocks[dref.index()] = Block(memory = mem, ownership = None)

    # def alloc(self, size):
    #     self.blocks[dref.index()] = OwnedMemory(None, None,

    def get_memory(self, dref):
        return self.blocks[dref.index()].memory

    def ensure_deserialized(self, dref):
        blk_mem = self.get_memory(dref)
        if not blk_mem.has_value:
            blk_mem.value = taskloaf.serialize.loads(
                self.worker, blk_mem.serialized.dereference(self.alloc.mem)
            )

    def get_local(self, dref):
        self.ensure_deserialized(dref)
        return self.get_memory(dref).value

    def get_serialized(self, dref):
        blk_mem = self.get_memory(dref)
        if not blk_mem.has_serialized:
            serialized_mem = memoryview(taskloaf.serialize.dumps(blk_mem.value))
            blk_mem.serialized = ShmemPtr(
                True,
                *self.alloc.store(serialized_mem)
            )
        return blk_mem.serialized

    def available(self, dref):
        return dref.index() in self.blocks

    def delete(self, dref):
        del self.blocks[dref.index()]

    def dec_ref_owned(self, creator, _id, gen, n_children):
        idx = (creator, _id)
        refcount = self.blocks[idx].ownership
        refcount.dec_ref(gen, n_children)
        if not refcount.alive():
            del self.blocks[idx]

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

class RemoteShmemRepo:
    def __init__(self, exit_stack):
        self.exit_stack = exit_stack
        self.remotes = dict()

    def get(self, dref):
        if dref.owner not in self.remotes:
            self.remotes[dref.owner] = self.exit_stack.enter_context(
                taskloaf.shmem.Shmem('/dev/shm/taskloaf' + str(dref.owner))
            )
        shmem = self.remotes[dref.owner]
        return dref.shmem_ptr.dereference(shmem.mem)
