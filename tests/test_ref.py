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
    for i in range(250):
        await asyncio.sleep(0.01)

@mpi_procs(2)
def test_submit_ref_work():
    async def f(w):
        ref = put(w, 1)
        assert(ref._id == 0)
        async def g(w):
            assert(ref._id == 0)
            async def h(w):
                print("SHUTDOWN")
                await taskloaf.worker.shutdown(w)
            print("SUBMIT TO 0")
            submit_ref_work(w, 0, h)
        print("SUBMIT TO 1")
        submit_ref_work(w, 1, g)
        print("HI!")
        await wait_to_die()
        print("BYE!")
    cluster(2, f)

@mpi_procs(2)
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
            print('start g!')
            assert(await ref.get() == 1)
            assert(ref.key() in w.object_cache)
            assert(await ref2.get() == one_serialized)
            assert(ref2.key() in w.object_cache)

            v1 = w.start_free_task(ref._remote_get())
            await asyncio.sleep(0)
            assert(isinstance(w.object_cache[ref.key()], asyncio.Future))
            assert(await ref.get() == 1)
            assert(isinstance(w.object_cache[ref.key()], int))
            assert(await v1 == 1)
            async def h(w):
                print("SHUTDOWN 0")
                await taskloaf.worker.shutdown(w)
            print("submit shutdown to 0")
            submit_ref_work(w, 0, h)
        print('submit g to 1')
        submit_ref_work(w, 1, g)
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
            async def h(w):
                if not hasattr(w, 'x'):
                    w.x = 0
                else:
                    await taskloaf.worker.shutdown(w)
            submit_ref_work(0, h)
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

# @mpi_procs(3)
# def test_multiuse_msgs():
#     """
#     Here, ref is deserialized multiple times on different workers.
#     But, ref is only serialized (and the child count incremented)
#     once on the main worker.
#     So, the ref count will be 2 while there will be 3 live references.
#     """
#     async def f(w):
#         ref = put(w, 1)
#         print('refid', ref._id)
#         async def fnc():
#             print('fnc')
#             assert(await ref.get() == 1)
#         ref_fnc = put(w, fnc)
#         print('ref_fnc id', ref_fnc._id)
#         async def g(w):
#             await (await ref_fnc.get())()
#         w.submit_work(1, g)
#         w.submit_work(2, g)
#         w.submit_work(1, taskloaf.worker.shutdown)
#         w.submit_work(2, taskloaf.worker.shutdown)
#         del ref
#         await wait_to_die()
#     cluster(3, f)

if __name__ == "__main__":
    test_multiuse_msgs()
