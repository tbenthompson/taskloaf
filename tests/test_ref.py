import gc
import pickle
import pytest
import asyncio
from taskloaf.test_decorators import mpi_procs
from taskloaf.cluster import cluster
from taskloaf.ref import *
from fixtures import w

def syncawait(coro):
    return asyncio.get_event_loop().run_until_complete(coro)

def test_make_ref(w):
    r = Ref(w, [12,3,4])
    assert(syncawait(r.get()) == [12,3,4])

def test_gcref_get(w):
    r = Ref(w, [12,3,4])
    gcr = r.convert()
    assert(syncawait(gcr.get()) == [12,3,4])
    w.object_cache.clear()
    assert(syncawait(gcr.get()) == [12,3,4])

def test_gcref_delete(w):
    assert(len(w.ref_manager.entries) == 0)
    r = Ref(w, 10)
    assert(len(w.ref_manager.entries) == 0)
    gcr = r.convert()
    assert(len(w.ref_manager.entries) == 1)
    del r
    assert(len(w.ref_manager.entries) == 1)
    del gcr
    assert(len(w.ref_manager.entries) == 0)

def test_ref_conversion_caching(w):
    r = Ref(w, [12,3,4])
    gcr = r.convert()
    assert(isinstance(gcr, GCRef))
    gcr2 = r.convert()
    assert(gcr is gcr2)

def test_ref_pickle_exception(w):
    r = Ref(w, [12,3,4])
    gcr = r.convert()

    with pytest.raises(Exception):
        pickle.dumps(r)

    with pytest.raises(Exception):
        pickle.dumps(gcr)

def ref_serialization_tester(w, sfnc, dfnc):
    assert(len(w.ref_manager.entries) == 0)
    ref = Ref(w, 10)
    gcr = ref.convert()
    assert(len(w.ref_manager.entries) == 1)
    ref_bytes = sfnc(gcr)
    del ref
    del gcr
    assert(len(w.ref_manager.entries) == 1)
    gcr2 = dfnc(w, ref_bytes)
    assert(syncawait(gcr2.get()) == 10)
    del w.object_cache[gcr2.key()]
    assert(syncawait(gcr2.get()) == 10)

    del gcr2
    gc.collect()
    assert(len(w.ref_manager.entries) == 0)

def test_ref_pickle_delete(w):
    from taskloaf.serialize import dumps, loads
    ref_serialization_tester(w, dumps, loads)

def test_ref_encode_capnp(w):
    def serialize(ref):
        m = taskloaf.message_capnp.GCRef.new_message()
        ref.encode_capnp(m)
        return m.to_bytes()
    def deserialize(w, ref_b):
        m = taskloaf.message_capnp.GCRef.from_bytes(ref_b)
        return GCRef.decode_capnp(w, m)
    ref_serialization_tester(w, serialize, deserialize)

def test_alloc(w):
    ref = alloc(w, 100)
    assert(len(syncawait(ref.get())) == 100)

async def wait_to_die():
    for i in range(25):
        await asyncio.sleep(0.01)

@mpi_procs(2)
def test_get():
    one_serialized = taskloaf.serialize.dumps(1)
    async def f(w):
        ref = put(w, 1)
        ref2 = alloc(w, len(one_serialized))
        buf = await ref2.get()
        buf[:] = one_serialized
        assert(await ref.get() == 1)
        assert(await ref2.get() == one_serialized)
        async def g(w):
            assert(await ref.get() == 1)
            assert(ref.key() in w.object_cache)
            assert(await ref2.get() == one_serialized)
            assert(ref2.key() in w.object_cache)

            v1 = asyncio.ensure_future(ref._remote_get())
            await asyncio.sleep(0)
            assert(isinstance(w.object_cache[ref.key()], asyncio.Future))
            assert(await ref.get() == 1)
            assert(isinstance(w.object_cache[ref.key()], int))
            assert(await v1 == 1)
            def h(w):
                taskloaf.worker.shutdown(w)
            w.submit_work(0, h)
        w.submit_work(1, g)
        await wait_to_die()
    cluster(2, f)

@mpi_procs(2)
def test_remote_double_get():
    async def f(w):
        ref = put(w, 1)
        async def g(w):
            assert(await ref.get() == 1)
            v1 = ref._remote_get()
            assert(await v1 == 1)
            def h(w):
                if not hasattr(w, 'x'):
                    w.x = 0
                else:
                    taskloaf.worker.shutdown(w)
            w.submit_work(0, h)
        w.submit_work(1, g)
        w.submit_work(1, g)
        await wait_to_die()
    cluster(2, f)

@mpi_procs(3)
def test_multiuse_msgs():
    """
    Here, ref is deserialized multiple times on different workers.
    But, ref is only serialized (and the child count incremented)
    once on the main worker.
    So, the ref count will be 2 while there will be 3 live references.
    """
    async def f(w):
        ref = put(w, 1)
        print('refid', ref._id)
        async def fnc():
            print('fnc')
            assert(await ref.get() == 1)
        ref_fnc = put(w, fnc)
        print('ref_fnc id', ref_fnc._id)
        async def g(w):
            await (await ref_fnc.get())()
        w.submit_work(1, g)
        w.submit_work(2, g)
        w.submit_work(1, taskloaf.worker.shutdown)
        w.submit_work(2, taskloaf.worker.shutdown)
        del ref
        await wait_to_die()
    cluster(3, f)

if __name__ == "__main__":
    test_multiuse_msgs()
