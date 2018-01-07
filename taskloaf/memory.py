import capnp
import asyncio
import taskloaf.message_capnp
import taskloaf.shmem
from taskloaf.dref import DistributedRef, ShmemPtr

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

class RemoteMemory:
    def __init__(self, value, serialized, store):
        self.store = store
        if value is not None:
            self.value = value
        elif serialized is not None:
            self.serialized = ShmemPtr(True, *self.store.store(serialized))
        else:
            self.value = None

    def has_serialized(self):
        return hasattr(self, 'serialized')

    def get_serialized(self):
        if not self.has_serialized():
            if is_bytes(self.value):
                self.serialized = ShmemPtr(
                    False,
                    *self.store.store(self.value)
                )
            else:
                self.serialized = ShmemPtr(
                    True,
                    *self.store.store(taskloaf.serialize.dumps(self.value))
                )
        return (
            self.serialized.needs_deserialize,
            self.store.dereference(self.serialized)
        )

    def get_value(self, worker):
        if not hasattr(self, 'value'):
            self.value = taskloaf.serialize.loads(
                worker, self.store.dereference(self.serialized)
            )
        return self.value

class OwnedMemory(RemoteMemory):
    def __init__(self, value, serialized, store):
        self.refcount = RefCount()
        super().__init__(value, serialized, store)

class SerializedMemoryStore:
    def __init__(self, addr, exit_stack):
        size = int(1e8)
        filename = 'taskloaf' + str(addr)
        self.shmem_filepath = exit_stack.enter_context(
            taskloaf.shmem.alloc_shmem(size, filename)
        )
        self.shmem = exit_stack.enter_context(
            taskloaf.shmem.Shmem(self.shmem_filepath)
        )
        self.ptr = 0

    def store(self, bytes):
        size = len(bytes)
        old_ptr = self.ptr
        next_ptr = self.ptr + size
        self.shmem.mem[self.ptr:next_ptr] = bytes
        self.ptr = next_ptr
        return old_ptr, next_ptr

    def dereference(self, ptr):
        return ptr.dereference(self.shmem.mem)

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
            handler = lambda w, x: lambda worker: worker.memory._dec_ref_owned(*x)
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

    def get_new_id(self):
        self.next_id += 1
        return self.next_id - 1

    def put(self, *, value = None, serialized = None, dref = None):
        putter = self._put_owned
        if dref is None:
            dref = DistributedRef(self.worker, self.worker.addr)
        elif dref.owner != self.worker.addr:
            putter = self._put_remote

        putter(value, serialized, dref)
        return dref

    def _put_owned(self, value, serialized, dref):
        mem = OwnedMemory(value, serialized, self.store)
        if mem.has_serialized():
            dref.shmem_ptr = mem.serialized
        self.blocks[dref.index()] = mem

    def _put_remote(self, value, serialized, dref):
        self.blocks[dref.index()] = RemoteMemory(value, serialized, self.store)

    def get(self, dref):
        if dref.index() in self.blocks:
            return self.blocks[dref.index()].get_value(dref.worker)
        else:
            if dref.owner not in self.remote_shmem:
                exit_stack = self.worker.exit_stack
                self.remote_shmem[dref.owner] = exit_stack.enter_context(
                    taskloaf.shmem.Shmem('/dev/shm/taskloaf' + str(dref.owner))
                )
            shmem = self.remote_shmem[dref.owner]
            v = dref.shmem_ptr.dereference(shmem.mem)
            if dref.shmem_ptr.needs_deserialize:
                self.put(serialized = v, dref = dref)
                return self.get(dref)
            else:
                return v

    def get_serialized(self, dref):
        return self.blocks[dref.index()].get_serialized()

    def available(self, dref):
        return (
            dref.index() in self.blocks or
            not dref.shmem_ptr.is_null()
        )

    def delete(self, dref):
        del self.blocks[dref.index()]

    def _dec_ref_owned(self, creator, _id, gen, n_children):
        idx = (creator, _id)
        mem = self.blocks[idx].refcount
        mem.dec_ref(gen, n_children)
        if not mem.alive():
            del self.blocks[idx]

    def dec_ref(self, creator, _id, gen, n_children, owner):
        if owner != self.worker.addr:
            self.worker.send(
                owner, self.worker.protocol.DECREF,
                (creator, _id, gen, n_children)
            )
        else:
            self._dec_ref_owned(creator, _id, gen, n_children)

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

    def n_entries(self):
        return len(self.blocks)

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

async def remote_get(worker, dref):
    mm = worker.memory

    if not mm.available(dref):
        mm.put(value = asyncio.Future(), dref = dref)
        worker.send(dref.owner, worker.protocol.REMOTEGET, [dref])

    val = mm.get(dref)
    if isinstance(val, asyncio.Future):
        await val
        return mm.get(dref)
    else:
        return val

def handle_remote_put(worker, args):
    dref, needs_deserialize, v = args
    def run(w):
        mm = w.memory
        mm.get(dref).set_result(None)
        if needs_deserialize:
            mm.put(serialized = v, dref = dref)
        else:
            mm.put(value = v, dref = dref)
    return run

class RemotePutSerializer:
    @staticmethod
    def serialize(args):
        dref, needs_deserialize, v = args
        m = taskloaf.message_capnp.Message.new_message()
        m.init('remotePut')
        dref.encode_capnp(m.remotePut.dref)
        m.remotePut.needsDeserialize = needs_deserialize
        m.remotePut.val = v
        return m

    @staticmethod
    def deserialize(w, m):
        return (
            DistributedRef.decode_capnp(w, m.remotePut.dref),
            m.remotePut.needsDeserialize,
            m.remotePut.val
        )

def handle_remote_get(worker, args):
    dref = args[0]
    source_addr = worker.cur_msg.sourceAddr
    def run(worker):
        needs_deserialize, v = worker.memory.get_serialized(dref)
        args = [dref, needs_deserialize, v]
        worker.send(source_addr, worker.protocol.REMOTEPUT, args)
    return run
