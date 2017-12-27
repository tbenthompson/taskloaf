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
from taskloaf.memory import Ref
from taskloaf.promise import encode_ref
import taskloaf.serialize
from fakes import fake_worker, FakePromise

# TODO: I think I should add the shmemPtr to the ref itself?
# TODO: The situation with a task launch where the creator != owner leads to some difficulties. This is a weird semi-initialized state. It needs to be somewhat similar to a normal promise in that it should be await-able.

def send_message(refs):
    m = taskloaf.message_capnp.Message.new_message()
    m.init('chunks', len(refs))
    for i in range(len(refs)):
        encode_ref(m.chunks[i], refs[i])
        # m.chunks[i].shmemPtr =


def test_build_message():
    worker = fake_worker()
    ref = Ref(worker, worker.addr)

    m = taskloaf.message_capnp.Message.new_message()
    import ipdb
    ipdb.set_trace()
    m.init('chunks', 1)
    encode_ref(m.chunks[0], ref)
    m.chunks[0].shmemPtr = 0
    m.chunks[0].valIncluded = True
    m.chunks[0].val = taskloaf.serialize.dumps(lambda x: x * 3)

    m_in = taskloaf.message_capnp.Message.from_bytes(m.to_bytes())
    assert(taskloaf.serialize.loads(None, m_in.chunks[0].val)(9) == 27)
