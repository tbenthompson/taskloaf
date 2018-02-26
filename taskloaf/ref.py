import asyncio
import taskloaf.serialize

def put(worker, obj):
    return Ref(worker, obj)

def alloc(worker, nbytes):
    return first_gcref(
        worker, worker.get_new_id(),
        worker.allocator.malloc(nbytes), False
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
        self.gcref = None

    async def get(self):
        return self.obj

    def __getstate__(self):
        raise Exception()

    def convert(self):
        if self.gcref is not None:
            return self.gcref
        else:
            return self._new_ref()

    def _new_ref(self):
        deserialize, serialized_obj = serialize_if_needed(self.obj)
        nbytes = len(serialized_obj)
        ptr = self.worker.allocator.malloc(nbytes)
        ptr.deref()[:] = serialized_obj
        self.worker.object_cache[(self.worker.addr, self._id)] = self.obj
        self.gcref = first_gcref(self.worker, self._id, ptr, deserialize)
        return self.gcref

def is_bytes(v):
    return isinstance(v, bytes) or isinstance(v, memoryview)

def serialize_if_needed(obj):
    if is_bytes(obj):
        return False, obj
    else:
        return True, taskloaf.serialize.dumps(obj)

def first_gcref(worker, _id, ptr, deserialize):
    ref = GCRef(worker, _id, ptr, deserialize)
    worker.ref_manager.new_ref(_id)
    return ref

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

    async def get(self):
        local = False
        if self.key() in self.worker.object_cache:
            val = self.worker.object_cache[self.key()]
            if isinstance(val, asyncio.Future):
                return (await val)
            else:
                return val

        ptr_accessible = self.ptr is not None
        if ptr_accessible:
            return self._deserialize_and_store(self.ptr.deref())
        else:
            return (await self._remote_get())

    async def _remote_get(self):
        future = asyncio.Future()
        self.worker.object_cache[self.key()] = future
        self.worker.send(self.owner, self.worker.protocol.REMOTEGET, [self])
        return (await future)

    def _remote_put(self, buf):
        future = self.worker.object_cache[self.key()]
        obj = self._deserialize_and_store(buf)
        future.set_result(obj)

    def key(self):
        return (self.owner, self._id)

    def _deserialize_and_store(self, buf):
        if self.deserialize:
            out = taskloaf.serialize.loads(self.worker, buf)
        else:
            out = buf
        self.worker.object_cache[self.key()] = out
        return out

    def __del__(self):
        self.worker.ref_manager.dec_ref(
            self._id, self.gen, self.n_children,
            owner = self.owner
        )

    # TODO: After encoding or pickling, the __del__ call will only happen if
    # the object is decoded properly and nothing bad happens. This is scary,
    # but is hard to avoid without adding a whole lot more synchronization and
    # tracking. It would be worth adding some tools to check that memory isn't
    # being leaked. Maybe a global counter of number of encoded and decoded
    # drefs (those #s should be equal).
    def encode_capnp(self, dest):
        self.n_children += 1
        dest.owner = self.owner
        dest.id = self._id
        dest.gen = self.gen + 1
        dest.deserialize = self.deserialize
        dest.ptr.start = self.ptr.start
        dest.ptr.end = self.ptr.end
        dest.ptr.blockIdx = self.ptr.block.idx

    @classmethod
    def decode_capnp(cls, worker, m):
        ref = GCRef.__new__(GCRef)
        ref.owner = m.owner
        ref._id = m.id
        ref.gen = m.gen
        ref.deserialize = m.deserialize
        ref.ptr = None #TODO
        ref.n_children = 0
        ref.worker = worker
        return ref

    def __getstate__(self):
        raise Exception()

def setup_protocol(worker):
    worker.protocol.add_msg_type(
        'REMOTEGET', type = GCRefListMsg, handler = handle_remote_get
    )
    worker.protocol.add_msg_type(
        'REMOTEPUT', type = RemotePutMsg, handler = handle_remote_put
    )

class GCRefListMsg:
    @staticmethod
    def serialize(refs):
        msg = taskloaf.message_capnp.Message.new_message()
        msg.init('refList', len(refs))
        for i in range(len(refs)):
            refs[i].encode_capnp(msg.refList[i])
        return msg

    @staticmethod
    def deserialize(worker, msg):
        return [GCRef.decode_capnp(worker, ref) for ref in msg.refList]

class RemotePutMsg:
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
    worker.send(
        worker.cur_msg.sourceAddr,
        worker.protocol.REMOTEPUT,
        [args[0], args[0].ptr.deref()]
    )
    return lambda: None

def handle_remote_put(worker, args):
    args[0]._remote_put(args[1])
    return lambda: None
