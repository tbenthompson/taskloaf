from taskloaf.protocol import Protocol

def test_roundtrip():
    p = Protocol()
    p.add_handler('simple')
    data = (123, 456)
    b = p.encode(p.simple, *data)
    out = p.decode(None, b)
    assert(out == (p.simple, data))

def test_work():
    p = Protocol()
    p.add_handler('simple')
    assert(p.get_name(p.simple) == 'simple')

    def set():
        set.val = 1
    set.val = 0
    p.build_work(0, [set])()
    assert(set.val == 1)

import capnp
import taskloaf.message_capnp

# TODO: Data transfer should use a pull mechanism. (vs push)
# Why pull? Pull is nice because then the sending worker doesn't need to know what data is available on the receiving worker. Pull does increase latency, but not by a huge amount since it only requires one extra round-trip. Assuming we're in a single data center, that should only be a few microseconds if the software/hardware are properly optimized. I would much rather than tackle the problem of doing extra round-trip messages quickly than the much harder problem of sharing state between the various worker across the system.
# Implementing a pull mechanism is easy. You just send a message to the owner of a data block asking for the relevant data. Then the worker at the other end of that request replies with the data you want. Within a node, a pull can be done without any communication by sharing a pointer to the location in a shared memory (mmap) block that. If I just always share shmem_ptrs, then this is easy.
# The presence of a shmem_ptr in every message also has an additional benefit in that this enables a remote request to be sent to any worker on a given node since they can all access the memory chunk. Theoretically interesting, but I doubt it'd be good to take advantage of. There's no way to know which workers on the node are less busy and it would be easy to accidentally incur some big NUMA costs.


from taskloaf.memory import Ref
from taskloaf.promise import encode_ref
import taskloaf.serialize
from fakes import fake_worker, FakePromise

def build_msg(refs, dest):
    # If the destination is

def test_message():
    worker = fake_worker()
    ref = Ref(worker, worker.addr)

    m = taskloaf.message_capnp.Message.new_message()
    m.init('chunks', 1)
    encode_promise(m.chunks[0].ref, ref)
    m.chunks[0].shmem_ptr = 0
    m.chunks[0].included = True
    m.chunks[0].val = taskloaf.serialize.dumps(lambda x: x * 3)
    m.to_bytes()
