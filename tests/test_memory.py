import gc
import asyncio

from taskloaf.serialize import dumps, loads
import taskloaf.message_capnp
from taskloaf.memory import *
from taskloaf.run import null_comm_worker
from taskloaf.cluster import cluster
from taskloaf.test_decorators import mpi_procs
from taskloaf.mpi import mpiexisting

one_serialized = dumps(1)

def dref_serialization_tester(sfnc, dfnc):
    w = null_comm_worker()
    mm = w.memory
    assert(mm.n_entries() == 0)
    dref = mm.put(value = 1)
    assert(mm.n_entries() > 0)
    dref_bytes = sfnc(dref)
    del dref
    assert(mm.n_entries() > 0)
    dref2 = dfnc(w, dref_bytes)
    del dref2
    gc.collect()
    assert(w.memory.n_entries() == 0)

def test_dref_pickle_delete():
    dref_serialization_tester(dumps, loads)

def test_dref_encode_capnp():
    def serialize(dref):
        m = taskloaf.message_capnp.DistributedRef.new_message()
        dref.encode_capnp(m)
        return m.to_bytes()
    def deserialize(w, dref_b):
        m = taskloaf.message_capnp.DistributedRef.from_bytes(dref_b)
        return DistributedRef.decode_capnp(w, m)
    dref_serialization_tester(serialize, deserialize)

def test_refcount_simple():
    rc = RefCount()
    assert(rc.alive())
    rc.dec_ref(0, 0)
    assert(not rc.alive())

def test_refcount_dead():
    rc = RefCount()
    rc.dec_ref(0, 2)
    rc.dec_ref(1, 0)
    rc.dec_ref(1, 0)
    assert(not rc.alive())

def test_refcount_alive():
    rc = RefCount()
    rc.dec_ref(0, 2)
    rc.dec_ref(1, 0)
    rc.dec_ref(1, 1)
    assert(rc.alive())

def test_put_get_delete():
    w = null_comm_worker()
    mm = w.memory
    dref = DistributedRef(w, w.addr + 1)
    mm.put(value = 1, dref = dref)
    assert(mm.available(dref))
    assert(mm.get(dref) == 1)
    mm.delete(dref)
    assert(not mm.available(dref))

def test_decref_local():
    w = null_comm_worker()
    mm = w.memory
    dref = mm.put(value = 1)
    assert(len(mm.blocks.keys()) == 1)
    del dref
    gc.collect() # Force a GC collect to make sure that dref.__del__ is called
    assert(len(mm.blocks.keys()) == 0)

def test_decref_encode():
    w = null_comm_worker()
    b = DecRefSerializer.serialize([1, 2, 3, 4]).to_bytes()
    m = taskloaf.message_capnp.Message.from_bytes(b)
    creator, _id, gen, n_children = DecRefSerializer.deserialize(w, m)
    assert(creator == 1)
    assert(_id == 2)
    assert(gen == 3)
    assert(n_children == 4)

@mpi_procs(2)
def test_remote_get():
    async def f(w):
        dref = w.memory.put(value = 1)
        dref2 = w.memory.put(value = one_serialized)
        dref3 = w.memory.put(serialized = one_serialized)
        assert(await remote_get(w, dref) == 1)
        assert(await remote_get(w, dref2) == one_serialized)
        assert(await remote_get(w, dref3) == 1)
        async def g(w):
            assert(await remote_get(w, dref) == 1)
            assert(await remote_get(w, dref2) == one_serialized)
            assert(await remote_get(w, dref3) == 1)
            def h(w):
                taskloaf.worker.shutdown(w)
            w.submit_work(0, h)
        w.submit_work(1, g)
        while True:
            await asyncio.sleep(0)
    cluster(2, f)

@mpi_procs(2)
def test_remote_double_get():
    async def f(w):
        dref = w.memory.put(value = 1)
        async def g(w):
            assert(await remote_get(w, dref) == 1)
        t1 = taskloaf.task(w, g, to = 1)
        t2 = taskloaf.task(w, g, to = 1)
        await t1
        await t2
    cluster(2, f)
