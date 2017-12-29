from taskloaf.protocol import *
from taskloaf.memory import DistributedRef, DRefListSerializer
from taskloaf.run import null_comm_worker

def test_roundtrip_default():
    p = Protocol()
    p.add_msg_type('simple')
    data = (123, 456)
    b = p.encode(p.simple, data)
    out = p.decode(None, b)
    assert(out == (p.simple, data))

def test_work():
    p = Protocol()
    p.add_msg_type('simple', handler = lambda x: x)
    assert(p.get_name(p.simple) == 'simple')
    def f():
        f.val = 1
    f.val = 0
    p.handle(0, f)()
    assert(f.val == 1)

def test_encode_decode():
    w = null_comm_worker()
    p = Protocol()
    p.add_msg_type('simple', serializer = DRefListSerializer)
    drefs = [DistributedRef(w, w.addr + 1) for i in range(3)]
    b = p.encode(p.simple, drefs)
    type_code, new_drefs = p.decode(w, b)
    for dr1, dr2 in zip(drefs, new_drefs):
        assert(dr1._id == dr2._id)



# import capnp
#
# import taskloaf.message_capnp
# from taskloaf.memory import DistributedRef
# from taskloaf.promise import encode_dref
# import taskloaf.serialize
#
# # TODO: I think I should add the shmemPtr to the dref itself?
# # TODO: The situation with a task launch where the creator != owner leads to some difficulties. This is a weird semi-initialized state. It needs to be somewhat similar to a normal promise in that it should be await-able.
#
# def send_message(drefs):
#     m = taskloaf.message_capnp.Message.new_message()
#     m.init('chunks', len(drefs))
#     for i in range(len(drefs)):
#         encode_dref(m.chunks[i], drefs[i])
#         # m.chunks[i].shmemPtr =
#
#
# def test_build_message():
#     worker = fake_worker()
#     dref = DistributedRef(worker, worker.addr)
#
#     m = taskloaf.message_capnp.Message.new_message()
#     import ipdb
#     ipdb.set_trace()
#     m.init('chunks', 1)
#     encode_dref(m.chunks[0], dref)
#     m.chunks[0].shmemPtr = 0
#     m.chunks[0].valIncluded = True
#     m.chunks[0].val = taskloaf.serialize.dumps(lambda x: x * 3)
#
#     m_in = taskloaf.message_capnp.Message.from_bytes(m.to_bytes())
#     assert(taskloaf.serialize.loads(None, m_in.chunks[0].val)(9) == 27)
