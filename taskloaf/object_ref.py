import asyncio
import taskloaf.serialize
import taskloaf.allocator
import taskloaf.refcounting


def put(obj):
    return FreshObjectRef(obj)


def alloc(nbytes):
    ptr = taskloaf.ctx().allocator.malloc(nbytes)

    def on_delete(_id, ptr=ptr):
        taskloaf.ctx().allocator.free(ptr)

    ref = taskloaf.refcounting.Ref(on_delete)
    return ObjectRef(ref, ptr, False)


def submit_ref_work(to, f):
    ref = put(f).convert()
    taskloaf.ctx().messenger.send(
        to, taskloaf.ctx().protocol.REFWORK, [ref, b""]
    )


def handle_ref_work(args):
    f_ref = args[1]

    async def run_me():
        f = await f_ref.get()
        await taskloaf.ctx().executor.wait_for_work(f)

    taskloaf.ctx().executor.start_async_work(run_me)


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
    def __init__(self, obj):
        self._id = taskloaf.ctx().get_new_id()
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
        deserialize, child_refs, serialized_obj = serialize_if_needed(self.obj)
        nbytes = len(serialized_obj)
        ptr = taskloaf.ctx().allocator.malloc(nbytes)
        ptr.deref()[:] = serialized_obj
        taskloaf.ctx().object_cache[(taskloaf.ctx().name, self._id)] = self.obj

        def on_delete(_id, ptr=ptr):
            key = (taskloaf.ctx().name, _id)
            del taskloaf.ctx().object_cache[key]
            taskloaf.ctx().allocator.free(ptr)

        ref = taskloaf.refcounting.Ref(
            on_delete, _id=self._id, child_refs=child_refs
        )
        self.objref = ObjectRef(ref, ptr, deserialize)
        return self.objref

    def encode_capnp(self, msg):
        self.convert().encode_capnp(msg)


def is_bytes(v):
    return isinstance(v, bytes) or isinstance(v, memoryview)


def serialize_if_needed(obj):
    if is_bytes(obj):
        return False, [], obj
    else:
        child_refs, blob = taskloaf.serialize.dumps(obj)
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

    async def get(self):
        await self._ensure_available()
        self._ensure_deserialized()
        return self.get_local()

    async def get_buffer(self):
        await self._ensure_available()
        return self.ptr.deref()

    def get_local(self):
        return taskloaf.ctx().object_cache[self.key()]

    async def _ensure_available(self):
        self.ref._ensure_child_refs_deserialized()
        if self.key() in taskloaf.ctx().object_cache:
            val = taskloaf.ctx().object_cache[self.key()]
            if isinstance(val, asyncio.Future):
                await val

        ptr_accessible = self.ptr is not None
        if not ptr_accessible:
            await self._remote_get()

    def _ensure_deserialized(self):
        if self.key() not in taskloaf.ctx().object_cache:
            self._deserialize_and_store(self.ptr.deref())

    async def _remote_get(self):
        future = asyncio.Future(loop=taskloaf.ctx().executor.ioloop)
        taskloaf.ctx().object_cache[self.key()] = future
        taskloaf.ctx().send(
            self.ref.owner, taskloaf.ctx().protocol.REMOTEGET, [self, b""]
        )
        return await future

    def _remote_put(self, buf):
        future = taskloaf.ctx().object_cache[self.key()]
        obj = self._deserialize_and_store(buf)
        future.set_result(obj)

    def _deserialize_and_store(self, buf):
        if self.deserialize:
            assert isinstance(self.ref.child_refs, list)
            out = taskloaf.serialize.loads(self.ref.child_refs, buf)
        else:
            out = buf
        taskloaf.ctx().object_cache[self.key()] = out
        return out

    def __getstate__(self):
        return dict(ref=self.ref, deserialize=self.deserialize, ptr=self.ptr)

    def encode_capnp(self, msg):
        self.ref.encode_capnp(msg.ref)
        msg.deserialize = self.deserialize
        self.ptr.encode_capnp(msg.ptr)

    @classmethod
    def decode_capnp(cls, msg):
        objref = ObjectRef.__new__(ObjectRef)
        objref.ref = taskloaf.refcounting.Ref.decode_capnp(msg.ref)
        objref.deserialize = msg.deserialize
        objref.ptr = taskloaf.allocator.Ptr.decode_capnp(
            objref.ref.owner, msg.ptr
        )

        return objref


class ObjectMsg:
    @staticmethod
    def serialize(args):
        ref, v = args
        m = taskloaf.message_capnp.Message.new_message()
        m.init("object")
        ref.encode_capnp(m.object.objref)
        m.object.val = bytes(v)
        return m

    @staticmethod
    def deserialize(msg):
        return (
            msg.sourceName,
            ObjectRef.decode_capnp(msg.object.objref),
            msg.object.val,
        )


def handle_remote_get(args):
    source_name, objref, val = args

    async def reply(w):
        taskloaf.ctx().messenger.send(
            source_name,
            taskloaf.ctx().protocol.REMOTEPUT,
            [objref, await objref.get_buffer()],
        )

    taskloaf.ctx().executor.run_work(reply)


def handle_remote_put(args):
    args[1]._remote_put(args[2])
