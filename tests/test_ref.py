import gc
import pickle
import pytest
import asyncio
from taskloaf.test_decorators import mpi_procs
from taskloaf.run import null_comm_worker
from taskloaf.cluster import cluster
from taskloaf.ref import *

@pytest.fixture(scope = "function")
def w():
    with null_comm_worker() as worker:
        yield worker

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
            assert(await ref2.get() == one_serialized)
            def h(w):
                taskloaf.worker.shutdown(w)
            w.submit_work(0, h)
        w.submit_work(1, g)
        while True:
            print("HI")
            await asyncio.sleep(0.5)
    cluster(2, f)
