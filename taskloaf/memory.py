import capnp
import asyncio
import taskloaf.message_capnp
from taskloaf.dref import DistributedRef

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
    def __init__(self, value, serialized):
        if value is not None:
            self.value = value
        if serialized is not None:
            self.serialized = serialized
            self.needs_deserialize = True

    def get_serialized(self):
        if not hasattr(self, 'serialized'):
            if is_bytes(self.value):
                self.serialized = self.value
                self.needs_deserialize = False
            else:
                self.serialized = taskloaf.serialize.dumps(self.value)
                self.needs_deserialize = True
        return self.needs_deserialize, self.serialized

    def get_value(self, worker):
        if not hasattr(self, 'value'):
            self.value = taskloaf.serialize.loads(worker, self.serialized)
        return self.value

class OwnedMemory(RemoteMemory):
    def __init__(self, value, serialized):
        self.refcount = RefCount()
        super().__init__(value, serialized)


# The value that is "put" into the memory manager should always be the same as
# the value that is "get" from the memory manager, even if the object must be
# serialized and sent across the network.
class MemoryManager:
    def __init__(self, worker):
        self.blocks = dict()
        self.worker = worker
        self.worker.protocol.add_msg_type(
            'DECREF',
            serializer = DecRefSerializer,
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

    def get_new_id(self):
        self.next_id += 1
        return self.next_id - 1

    def put(self, *, value = None, serialized = None, dref = None):
        if dref is None:
            dref = DistributedRef(self.worker, self.worker.addr)
            self._put_owned(value, serialized, dref)
        elif dref.owner == self.worker.addr:
            self._put_owned(value, serialized, dref)
        else:
            self.blocks[dref.index()] = RemoteMemory(value, serialized)
        return dref

    def _put_owned(self, value, serialized, dref):
        self.blocks[dref.index()] = OwnedMemory(value, serialized)

    def get(self, dref):
        return self.blocks[dref.index()].get_value(dref.worker)

    def get_serialized(self, dref):
        return self.blocks[dref.index()].get_serialized()

    def available(self, dref):
        return dref.index() in self.blocks

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

def handle_remote_put(worker, args):
    dref, needs_deserialize, v = args
    def run(w):
        w.memory.get(dref).set_result((needs_deserialize, v))
    return run

def handle_remote_get(worker, args):
    dref = args[0]
    source_addr = worker.cur_msg.sourceAddr
    def run(worker):
        needs_deserialize, v = worker.memory.get_serialized(dref)
        args = [dref, needs_deserialize, v]
        worker.send(source_addr, worker.protocol.REMOTEPUT, args)
    return run

async def remote_get(worker, dref):
    mm = worker.memory
    if not mm.available(dref):
        mm.put(value = asyncio.Future(), dref = dref)
        worker.send(dref.owner, worker.protocol.REMOTEGET, [dref])
        needs_deserialize, v = await mm.get(dref)
        if needs_deserialize:
            mm.put(serialized = v, dref = dref)
        else:
            mm.put(value = v, dref = dref)
    return mm.get(dref)
