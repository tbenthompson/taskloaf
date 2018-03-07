import gc
import pickle
import pytest
import asyncio
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
    def check_norefs(v):
        assert(w.allocator.empty() == v)
        assert((len(w.ref_manager.entries) == 0) == v)

    check_norefs(True)
    r = Ref(w, 10)
    check_norefs(True)
    gcr = r.convert()
    check_norefs(False)
    del r
    check_norefs(False)
    del gcr
    check_norefs(True)

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
    assert(len(w.object_cache) == 0)

def test_ref_encode_capnp(w):
    def serialize(ref):
        return GCRefListMsg.serialize([ref]).to_bytes()
    def deserialize(w, ref_b):
        m = taskloaf.message_capnp.Message.from_bytes(ref_b)
        return GCRefListMsg.deserialize(w, m)[0]
    ref_serialization_tester(w, serialize, deserialize)

def test_alloc(w):
    ref = alloc(w, 100)
    assert(len(syncawait(ref.get())) == 100)

async def wait_to_die():
    for i in range(2500):
        await asyncio.sleep(0.01)

def test_submit_ref_work():
    async def f(w):
        ref = put(w, 1)
        assert(ref._id == 0)
        async def g(w):
            assert(ref._id == 0)
            submit_ref_work(w, 0, taskloaf.worker.shutdown)
        submit_ref_work(w, 1, g)
        await wait_to_die()
    cluster(2, f)

def test_get():
    one_serialized = pickle.dumps(1)
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
            submit_ref_work(w, 0, taskloaf.worker.shutdown)
        submit_ref_work(w, 1, g)
        await wait_to_die()
    cluster(2, f)

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
            submit_ref_work(w, 0, h)
        submit_ref_work(w, 1, g)
        submit_ref_work(w, 1, g)
        await wait_to_die()
    cluster(2, f)

def test_put_delete_ref(w):
    def f():
        ref = put(w, 1)
        gcref = ref.convert()
        ref2 = put(w, gcref)
        gcref2 = ref2.convert()
        del ref, gcref, ref2, gcref2
        import gc; gc.collect()
    f()
    assert(len(w.ref_manager.entries) == 0)

def test_multiuse_msgs():
    """
    Here, ref is deserialized multiple times on different workers.
    But, ref is only serialized (and the child count incremented)
    once on the main worker.
    So, the ref count will be 2 while there will be 3 live references.
    """
    async def f(w):
        async def fnc():
            v = await fnc.ref.get()
            assert(v == 1)
            del fnc.ref
        fnc.ref = put(w, 1)
        w.finished = False
        async def g(w):
            fnc = await g.ref_fnc.get()
            await fnc()
            del fnc, g.ref_fnc
            def h(w):
                w.finished = True
            w.submit_work(0, h)
            w.object_cache.clear()
            import gc; gc.collect()
        g.ref_fnc = put(w, fnc)
        submit_ref_work(w, 1, g)
        submit_ref_work(w, 2, g)
        w.submit_work(1, taskloaf.worker.shutdown)
        w.submit_work(2, taskloaf.worker.shutdown)
        del fnc.ref, g.ref_fnc
        while not w.finished:
            await asyncio.sleep(0)
        for i in range(200):
            if not w.allocator.empty():
                await asyncio.sleep(0.01)
                continue
        assert(w.allocator.empty())
    cluster(3, f)
