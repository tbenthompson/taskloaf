import capnp
import taskloaf.message_capnp

class DistributedRef:
    def __init__(self, worker, owner):
        self.worker = worker
        self.owner = owner
        self.creator = worker.addr
        self._id = worker.memory.get_new_id()
        self.gen = 0
        self.n_children = 0
        self.shmem_ptr = 0

    def index(self):
        return (self.creator, self._id)

    def __getstate__(self):
        self.n_children += 1
        return dict(
            worker = self.worker,
            owner = self.owner,
            creator = self.creator,
            _id = self._id,
            gen = self.gen + 1,
            n_children = 0,
        )

    def __del__(self):
        self.worker.memory.dec_ref(
            self.creator, self._id, self.gen, self.n_children,
            owner = self.owner
        )

    # TODO: After encoding or pickling, the __del__ call will only happen if
    # the object is decoded properly and nothing bad happens. This is scary,
    # but is hard to avoid without adding a whole lot more synchronization and
    # tracking. It would be worth adding some tools to check that memory isn't
    # being leaked. Maybe a counter of number of encoded and decoded drefs
    # (those #s should be equal).
    def encode_capnp(self, dest):
        self.n_children += 1
        dest.owner = self.owner
        dest.creator = self.creator
        dest.id = self._id
        dest.gen = self.gen + 1
        dest.shmemPtr = self.shmem_ptr

    @classmethod
    def decode_capnp(cls, worker, m):
        dref = DistributedRef.__new__(DistributedRef)
        dref.owner = m.owner
        dref.creator = m.creator
        dref._id = m.id
        dref.gen = m.gen
        dref.shmem_ptr = m.shmemPtr
        dref.n_children = 0
        dref.worker = worker
        return dref

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

def decref_encoder(type_code, creator, _id, gen, n_children):
    m = taskloaf.message_capnp.DecRef.new_message()
    m.typeCode = type_code
    m.creator = creator
    m.id = _id
    m.gen = gen
    m.nchildren = n_children
    return m.to_bytes()

def decref_decoder(worker, b):
    decref_obj = taskloaf.message_capnp.DecRef.from_bytes(b)
    return decref_obj.creator, decref_obj.id, decref_obj.gen, decref_obj.nchildren

class MemoryManager:
    def __init__(self, worker):
        self.blocks = dict()
        self.worker = worker
        self.worker.protocol.add_handler(
            'DECREF',
            encoder = decref_encoder,
            decoder = decref_decoder,
            work_builder = lambda x: lambda worker: worker.memory._dec_ref_owned(*x)
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

# worker.send(owner,

async def remote_get(dref):
    if not dref.available():
        dref.put(asyncio.Future())
        worker.send(owner, self.worker.protocol.REMOTEGET, [dref])
        # dref.put(await task(dref.worker, lambda worker: dref.get(), to = dref.owner))
    return dref.get()
