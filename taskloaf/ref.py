import asyncio
import taskloaf.serialize
import taskloaf.allocator
import taskloaf.refcounting

def put(worker, obj):
    return FreshObjectRef(worker, obj)

def alloc(worker, nbytes):
    ptr = worker.allocator.malloc(nbytes)
    def on_delete(_id, worker = worker, ptr = ptr):
        worker.allocator.free(ptr)
    ref = taskloaf.refcounting.Ref(worker, on_delete)
    return ObjectRef(ref, ptr, False)

def submit_ref_work(worker, to, f):
    ref = put(worker, f).convert()
    worker.send(to, worker.protocol.REFWORK, [ref, b''])

def setup_plugin(worker):
    assert(hasattr(worker, 'allocator'))
    assert(hasattr(worker, 'ref_manager'))
    worker.object_cache = dict()
    worker.protocol.add_msg_type(
        'REMOTEGET', type = ObjectMsg, handler = handle_remote_get
    )
    worker.protocol.add_msg_type(
        'REMOTEPUT', type = ObjectMsg, handler = handle_remote_put
    )
    worker.protocol.add_msg_type(
        'REFWORK', type = ObjectMsg, handler = handle_ref_work
    )

def handle_ref_work(worker, args):
    f_ref = args[0]
    async def run_me(worker):
        f = await f_ref.get()
        await worker.wait_for_work(f)
    worker.start_async_work(run_me)

def is_ref(x):
    return isinstance(x, FreshObjectRef) or isinstance(x, ObjectRef)

"""
This class barely needs to do anything because python already uses reference
counting for GC. It just needs to make sure that once it is serialized, all
serialized versions with the same id point to the same serialized chunk of
memory. This is based on the observation that in order to access a chunk of
memory from another worker, the reference first has to arrive at that other
worker and thus serialization of the reference can be used as a trigger for
serialization of the underlying object.
"""
class FreshObjectRef:
    def __init__(self, worker, obj):
        self.worker = worker
        self._id = self.worker.get_new_id()
        self.obj = obj
        self.objref = None

    async def get(self):
        return self.get_local()

    def get_local(self):
        return self.obj

    def __reduce__(self):
        objref = self.convert()
        return (FreshObjectRef.reconstruct, (objref,))

    @classmethod
    def reconstruct(cls, objref):
        return objref

    def convert(self):
        if self.objref is not None:
            return self.objref
        else:
            return self._new_ref()

    def _new_ref(self):
        deserialize, child_refs, serialized_obj = serialize_if_needed(
            self.worker, self.obj
        )
        nbytes = len(serialized_obj)
        ptr = self.worker.allocator.malloc(nbytes)
        ptr.deref()[:] = serialized_obj
        self.worker.object_cache[(self.worker.addr, self._id)] = self.obj
        def on_delete(_id, worker = self.worker, ptr = ptr):
            key = (worker.addr, _id)
            del worker.object_cache[key]
            worker.allocator.free(ptr)
        ref = taskloaf.refcounting.Ref(
            self.worker, on_delete, _id = self._id, child_refs = child_refs
        )
        self.objref = ObjectRef(ref, ptr, deserialize)
        return self.objref

    def encode_capnp(self, msg):
        self.convert().encode_capnp(msg)

def is_bytes(v):
    return isinstance(v, bytes) or isinstance(v, memoryview)

def serialize_if_needed(worker, obj):
    if is_bytes(obj):
        return False, [], obj
    else:
        child_refs, blob = taskloaf.serialize.dumps(worker, obj)
        return True, child_refs, blob

"""
It seems like we're recording two indexes to the data:
    -- the ptr itself
    -- the (owner, _id) pair
This isn't strictly necessary, but has some advantages.
"""
class ObjectRef:
    def __init__(self, ref, ptr, deserialize):
        self.ref = ref
        self.ptr = ptr
        self.deserialize = deserialize

    def key(self):
        return self.ref.key()

    @property
    def worker(self):
        return self.ref.worker

    async def get(self):
        await self._ensure_available()
        self._ensure_deserialized()
        return self.get_local()

    async def get_buffer(self):
        await self._ensure_available()
        return self.ptr.deref()

    def get_local(self):
        return self.worker.object_cache[self.key()]

    async def _ensure_available(self):
        self.ref._ensure_child_refs_deserialized()
        if self.key() in self.worker.object_cache:
            val = self.worker.object_cache[self.key()]
            if isinstance(val, asyncio.Future):
                await val

        ptr_accessible = self.ptr is not None
        if not ptr_accessible:
            await self._remote_get()

    def _ensure_deserialized(self):
        if self.key() not in self.worker.object_cache:
            self._deserialize_and_store(self.ptr.deref())

    async def _remote_get(self):
        future = asyncio.Future()
        self.worker.object_cache[self.key()] = future
        self.worker.send(
            self.ref.owner, self.worker.protocol.REMOTEGET, [self, b'']
        )
        return (await future)

    def _remote_put(self, buf):
        future = self.worker.object_cache[self.key()]
        obj = self._deserialize_and_store(buf)
        future.set_result(obj)

    def _deserialize_and_store(self, buf):
        if self.deserialize:
            assert(isinstance(self.ref.child_refs, list))
            out = taskloaf.serialize.loads(
                self.ref.worker, self.ref.child_refs, buf
            )
        else:
            out = buf
        self.worker.object_cache[self.key()] = out
        return out

    def __getstate__(self):
        return dict(
            ref = self.ref,
            deserialize = self.deserialize,
            ptr = self.ptr,
        )

    def encode_capnp(self, msg):
        self.ref.encode_capnp(msg.ref)
        msg.deserialize = self.deserialize
        self.ptr.encode_capnp(msg.ptr)

    @classmethod
    def decode_capnp(cls, worker, msg):
        objref = ObjectRef.__new__(ObjectRef)
        objref.ref = taskloaf.refcounting.Ref.decode_capnp(worker, msg.ref)
        objref.deserialize = msg.deserialize
        objref.ptr = taskloaf.allocator.Ptr.decode_capnp(
            worker, objref.ref.owner, msg.ptr
        )

        return objref

class ObjectMsg:
    @staticmethod
    def serialize(args):
        ref, v = args
        m = taskloaf.message_capnp.Message.new_message()
        m.init('object')
        ref.encode_capnp(m.object.objref)
        m.object.val = bytes(v)
        return m

    @staticmethod
    def deserialize(worker, msg):
        return (
            ObjectRef.decode_capnp(worker, msg.object.objref),
            msg.object.val
        )

def handle_remote_get(worker, args):
    msg = worker.cur_msg
    async def reply(w):
        worker.send(
            msg.sourceAddr,
            worker.protocol.REMOTEPUT,
            [args[0], await args[0].get_buffer()]
        )
    worker.run_work(reply)

def handle_remote_put(worker, args):
    args[0]._remote_put(args[1])
