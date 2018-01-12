import asyncio
import taskloaf.message_capnp
from taskloaf.dref import DistributedRef

async def remote_get(worker, dref):
    await asyncio.sleep(0) #TODO: should get yield control?
    mm = worker.memory

    if mm.available(dref):
        val = mm.get_local(dref)
        if isinstance(val, asyncio.Future):
            await val
            return mm.get_local(dref)
        return val
    elif not dref.shmem_ptr.is_null():
        v = worker.remote_shmem.get(dref)
        if dref.shmem_ptr.needs_deserialize:
            mm.put(serialized = v, dref = dref)
            return mm.get_local(dref)
        else:
            return v
    else:
        mm.put(value = asyncio.Future(), dref = dref)
        worker.send(dref.owner, worker.protocol.REMOTEGET, [dref])
        await mm.get_local(dref)
        out = mm.get_local(dref)
        # if worker.addr == 1:
        #     print(dref.shmem_ptr, out)
        return out

def setup_protocol(worker):
    worker.protocol.add_msg_type(
        'REMOTEGET',
        serializer = DRefListSerializer,
        handler = handle_remote_get
    )
    worker.protocol.add_msg_type(
        'REMOTEPUT',
        serializer = RemotePutSerializer,
        handler = handle_remote_put
    )

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

def handle_remote_put(worker, args):
    dref, v = args
    def run(w):
        mm = w.memory
        mm.get_local(dref).set_result(None)
        v = w.remote_shmem.get(dref)
        if dref.shmem_ptr.needs_deserialize:
            mm.put(serialized = v, dref = dref)
        else:
            mm.put(value = v, dref = dref)
    return run

def handle_remote_get(worker, args):
    dref = args[0]
    source_addr = worker.cur_msg.sourceAddr
    def run(worker):
        dref.shmem_ptr = worker.memory.get_serialized(dref)
        args = [dref, bytes(0)]
        worker.send(source_addr, worker.protocol.REMOTEPUT, args)
    return run
