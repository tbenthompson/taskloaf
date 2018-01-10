import os
import attr
import capnp
import asyncio
import taskloaf.message_capnp
import taskloaf.shmem
from taskloaf.dref import DistributedRef, ShmemPtr

class SerializedMemoryStore:
    def __init__(self, addr, exit_stack):
        size = int(20e9)
        filename = 'taskloaf' + str(addr)
        try:
            os.remove('/dev/shm/' + filename)
        except FileNotFoundError:
            pass
        self.shmem_filepath = exit_stack.enter_context(
            taskloaf.shmem.alloc_shmem(size, filename)
        )
        self.shmem = exit_stack.enter_context(
            taskloaf.shmem.Shmem(self.shmem_filepath)
        )
        self.ptr = 0
        self.addr = addr

    def alloc(self, size):
        old_ptr = self.ptr
        next_ptr = self.ptr + size
        self.ptr = next_ptr
        return old_ptr, next_ptr

    def store(self, memory):
        size = memory.nbytes
        start_ptr, end_ptr = self.alloc(size)
        # print(self.addr, 'storing', size)
        if end_ptr > len(self.shmem.mem):
            raise Exception('out of memory!')
        if size > 1e5:
            with open(self.shmem_filepath, 'r+b') as f:
                f.seek(start_ptr)
                f.write(memory)
        else:
            self.shmem.mem[start_ptr:end_ptr] = memory
        return start_ptr, end_ptr

    def dereference(self, ptr):
        return ptr.dereference(self.shmem.mem)

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
    def __init__(self, worker, store):
        self.blocks = dict()
        self.worker = worker
        self.worker.protocol.add_msg_type(
            'DECREF',
            serializer = MemoryManager.DecRefSerializer,
            handler = lambda w, x: lambda worker: worker.memory.dec_ref_owned(*x)
        )
        self.worker.protocol.add_msg_type(
            'REMOTEGET',
            serializer = DRefListSerializer,
            handler = handle_remote_get
        )
        self.worker.protocol.add_msg_type(
            'REMOTEPUT',
            serializer = RemotePutSerializer,
            handler = handle_remote_put
        )
        self.next_id = 0
        self.store = store
        self.remote_shmem = dict()

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
                    *self.store.store(memoryview(value))
                ))
        elif serialized is not None:
            mem.set_serialized(ShmemPtr(True, *self.store.store(memoryview(serialized))))
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
                self.worker, self.store.dereference(blk_mem.serialized)
            )

    def get_local(self, dref):
        self.ensure_deserialized(dref)
        return self.get_memory(dref).value

    def get_shmem(self, dref):
        if dref.owner not in self.remote_shmem:
            exit_stack = self.worker.exit_stack
            self.remote_shmem[dref.owner] = exit_stack.enter_context(
                taskloaf.shmem.Shmem('/dev/shm/taskloaf' + str(dref.owner))
            )
        shmem = self.remote_shmem[dref.owner]
        return dref.shmem_ptr.dereference(shmem.mem)

    def get_serialized(self, dref):
        blk_mem = self.get_memory(dref)
        if not blk_mem.has_serialized:
            serialized_mem = memoryview(taskloaf.serialize.dumps(blk_mem.value))
            blk_mem.serialized = ShmemPtr(
                True,
                *self.store.store(serialized_mem)
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

class DRefListSerializer:
    @staticmethod
    def serialize(drefs):
        m = taskloaf.message_capnp.Message.new_message()
        m.init('drefList', len(drefs))
        for i in range(len(drefs)):
            drefs[i].encode_capnp(m.drefList[i])
        return m

    @staticmethod
    def deserialize(w, m):
        return [DistributedRef.decode_capnp(w, dr) for dr in m.drefList]

def put(worker, value = None, serialized = None):
    return worker.memory.put(value = value, serialized = serialized)

async def get(worker, dref):
    await asyncio.sleep(0) #TODO: should get yield control?
    mm = worker.memory

    if mm.available(dref):
        val = mm.get_local(dref)
        if isinstance(val, asyncio.Future):
            await val
            return mm.get_local(dref)
        return val
    elif not dref.shmem_ptr.is_null():
        v = mm.get_shmem(dref)
        if dref.shmem_ptr.needs_deserialize:
            mm.put(serialized = v, dref = dref)
            return mm.get_local(dref)
        else:
            return v
    else:
        mm.put(value = asyncio.Future(), dref = dref)
        worker.send(dref.owner, worker.protocol.REMOTEGET, [dref])
        await mm.get_local(dref)
        return mm.get_local(dref)

def handle_remote_put(worker, args):
    dref, v = args
    def run(w):
        mm = w.memory
        mm.get_local(dref).set_result(None)
        v = mm.get_shmem(dref)
        if dref.shmem_ptr.needs_deserialize:
            mm.put(serialized = v, dref = dref)
        else:
            mm.put(value = v, dref = dref)
    return run

class RemotePutSerializer:
    @staticmethod
    def serialize(args):
        dref, v = args
        m = taskloaf.message_capnp.Message.new_message()
        m.init('remotePut')
        dref.encode_capnp(m.remotePut.dref)
        m.remotePut.val = v
        return m

    @staticmethod
    def deserialize(w, m):
        return (
            DistributedRef.decode_capnp(w, m.remotePut.dref),
            m.remotePut.val
        )

def handle_remote_get(worker, args):
    dref = args[0]
    source_addr = worker.cur_msg.sourceAddr
    def run(worker):
        dref.shmem_ptr = worker.memory.get_serialized(dref)
        args = [dref, bytes(0)]
        worker.send(source_addr, worker.protocol.REMOTEPUT, args)
    return run
