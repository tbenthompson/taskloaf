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

class OwnedMemory:
    def __init__(self, value):
        self.value = value
        self.refcount = RefCount()

class RemoteMemory:
    def __init__(self, value):
        self.value = value

class DRefListSerializer:
    @staticmethod
    def serialize(type_code, drefs):
        m = taskloaf.message_capnp.Message.new_message()
        m.typeCode = type_code
        m.init('drefList', len(drefs))
        for i in range(len(drefs)):
            drefs[i].encode_capnp(m.drefList[i])
        return m.to_bytes()

    @staticmethod
    def deserialize(w, m):
        return [DistributedRef.decode_capnp(w, dr) for dr in m.drefList]

class DecRefSerializer:
    @staticmethod
    def serialize(type_code, args):
        m = taskloaf.message_capnp.Message.new_message()
        m.typeCode = type_code
        m.init('decRef')
        m.decRef.creator = args[0]
        m.decRef.id = args[1]
        m.decRef.gen = args[2]
        m.decRef.nchildren = args[3]
        return m.to_bytes()

    @staticmethod
    def deserialize(w, m):
        return m.decRef.creator, m.decRef.id, m.decRef.gen, m.decRef.nchildren

class RemotePutSerializer:
    @staticmethod
    def serialize(type_code, args):
        dref, v = args
        m = taskloaf.message_capnp.Message.new_message()
        m.typeCode = type_code
        m.init('remotePut')
        dref.encode_capnp(m.remotePut.dref)
        m.remotePut.val = v
        return m.to_bytes()

    @staticmethod
    def deserialize(w, m):
        return (
            DistributedRef.decode_capnp(w, m.remotePut.dref),
            m.remotePut.val
        )

class MemoryManager:
    def __init__(self, worker):
        self.blocks = dict()
        self.worker = worker
        self.worker.protocol.add_msg_type(
            'DECREF',
            serializer = DecRefSerializer,
            handler = lambda x: lambda worker: worker.memory._dec_ref_owned(*x)
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

    def put(self, v, dref = None):
        if dref is None:
            dref = DistributedRef(self.worker, self.worker.addr)
            self._put_owned(v, dref)
        elif dref.owner == self.worker.addr:
            self._put_owned(v, dref)
        else:
            self.blocks[dref.index()] = RemoteMemory(v)
        return dref

    def _put_owned(self, v, dref):
        self.blocks[dref.index()] = OwnedMemory(v)

    def get(self, dref):
        return self.blocks[dref.index()].value

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

def handle_remote_put(args):
    dref_dest, v = args
    def run(w):
        w.memory.get(dref_dest).set_result(v)
    return run

def handle_remote_get(args):
    dref_dest, dref = args
    def run(worker):
        v = worker.memory.get(dref)
        worker.send(dref_dest.owner, worker.protocol.REMOTEPUT, [dref_dest, v])
    return run

async def remote_get(worker, dref):
    mm = worker.memory
    if not mm.available(dref):
        dref_dest = mm.put(asyncio.Future())
        worker.send(dref.owner, worker.protocol.REMOTEGET, [dref_dest, dref])
        mm.put(await mm.get(dref_dest), dref = dref)
    return mm.get(dref)
