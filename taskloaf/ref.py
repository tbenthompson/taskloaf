import asyncio
import taskloaf.serialize
import taskloaf.allocator

def put(worker, obj):
    return Ref(worker, obj)

def alloc(worker, nbytes):
    return first_gcref(
        worker, worker.get_new_id(),
        worker.allocator.malloc(nbytes), False,
        []
    )

def submit_ref_work(worker, to, f):
    ref = put(worker, f).convert()
    worker.send(to, worker.protocol.REFWORK, [ref])

def setup_protocol(worker):
    worker.protocol.add_msg_type(
        'REMOTEGET', type = GCRefListMsg, handler = handle_remote_get
    )
    worker.protocol.add_msg_type(
        'REMOTEPUT', type = RemotePutMsg, handler = handle_remote_put
    )
    worker.protocol.add_msg_type(
        'REFWORK', type = GCRefListMsg, handler = handle_ref_work
    )

def handle_ref_work(worker, args):
    f_ref = args[0]
    async def run_me(worker):
        f = await f_ref.get()
        await worker.wait_for_work(f)
    worker.start_async_work(run_me)

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
        deserialize, ref_list, serialized_obj = serialize_if_needed(self.obj)
        nbytes = len(serialized_obj)
        ptr = self.worker.allocator.malloc(nbytes)
        ptr.deref()[:] = serialized_obj
        self.worker.object_cache[(self.worker.addr, self._id)] = self.obj
        self.gcref = first_gcref(self.worker, self._id, ptr, deserialize, ref_list)
        return self.gcref

def is_bytes(v):
    return isinstance(v, bytes) or isinstance(v, memoryview)

def serialize_if_needed(obj):
    if is_bytes(obj):
        return False, [], obj
    else:
        ref_list, blob = taskloaf.serialize.dumps(obj)
        return True, ref_list, blob

def first_gcref(worker, _id, ptr, deserialize, ref_list):
    ref = GCRef(worker, _id, ptr, deserialize, ref_list)
    worker.ref_manager.new_ref(_id, ptr)
    return ref

"""
It seems like we're recording two indexes to the data:
    -- the ptr itself
    -- the (owner, _id) pair
This isn't strictly necessary, but has some advantages.
"""
class GCRef:
    def __init__(self, worker, _id, ptr, deserialize, ref_list):
        self.worker = worker
        self.owner = worker.addr
        self._id = _id
        self.gen = 0
        self.n_children = 0
        self.ptr = ptr
        self.deserialize = deserialize
        self.ref_list = ref_list

    async def get(self):
        self._ensure_ref_list_deserialized()
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
            assert(isinstance(self.ref_list, list))
            out = taskloaf.serialize.loads(self.worker, self.ref_list, buf)
        else:
            out = buf
        self.worker.object_cache[self.key()] = out
        return out

    def __del__(self):
        self.worker.log.debug(
            '__del__', _id = self._id, gen = self.gen,
            n_children = self.n_children
        )
        self.worker.ref_manager.dec_ref(
            self._id, self.gen, self.n_children,
            owner = self.owner
        )

    def _ensure_ref_list_deserialized(self):
        if isinstance(self.ref_list, list):
            return
        self.ref_list = [
            GCRef.decode_capnp(self.worker, child_ref, child = True)
            for child_ref in self.ref_list
        ]

    def encode_reflist(self, msg):
        msg.init('refList', len(self.ref_list))
        if isinstance(self.ref_list, list):
            for i in range(len(self.ref_list)):
                self.ref_list[i].encode_capnp(msg.refList[i])
        else:
            msg.refList = self.ref_list

    # TODO: After encoding or pickling, the __del__ call will only happen if
    # the object is decoded properly and nothing bad happens. This is scary,
    # but is hard to avoid without adding a whole lot more synchronization and
    # tracking. It would be worth adding some tools to check that memory isn't
    # being leaked. Maybe a global counter of number of encoded and decoded
    # refs (those #s should be equal).
    def encode_capnp(self, msg):
        self.worker.log.debug(
            'copy', _id = self._id, n_children = self.n_children,
            gen = self.gen, owner = self.owner
        )
        self.n_children += 1
        msg.owner = self.owner
        msg.id = self._id
        msg.gen = self.gen + 1
        msg.deserialize = self.deserialize
        self.ptr.encode_capnp(msg.ptr)
        self.encode_reflist(msg)

    @classmethod
    def decode_capnp(cls, worker, msg, child = False):
        ref = GCRef.__new__(GCRef)
        ref.owner = msg.owner
        ref._id = msg.id
        ref.gen = msg.gen
        ref.deserialize = msg.deserialize
        ref.ptr = taskloaf.allocator.Ptr.decode_capnp(worker, ref.owner, msg.ptr)
        ref.n_children = 0
        ref.worker = worker
        ref.ref_list = msg.refList
        if not child:
            ref._ensure_ref_list_deserialized()
        return ref


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
        ref, v = args
        m = taskloaf.message_capnp.Message.new_message()
        m.init('remotePut')
        ref.encode_capnp(m.remotePut.ref)
        m.remotePut.val = bytes(v)
        return m

    @staticmethod
    def deserialize(worker, msg):
        return (
            GCRef.decode_capnp(worker, msg.remotePut.ref),
            msg.remotePut.val
        )

def handle_remote_get(worker, args):
    worker.send(
        worker.cur_msg.sourceAddr,
        worker.protocol.REMOTEPUT,
        [args[0], args[0].ptr.deref()]
    )

def handle_remote_put(worker, args):
    args[0]._remote_put(args[1])
