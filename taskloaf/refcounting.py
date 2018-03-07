"""
NOTE: Why is non-causality a problem for a incref/decref system?
v=1 refcount stored on node #3
ref A gets copied to ref B on node #1
ref B gets sent to node #2
ref A gets deleted on node #1
ref B gets used on node #2
ref B gets deleted on node #2
ref B deletion decref on node #3 --> v=0, object deleted!
ref B creation incref on node #3 --> error, object already deleted!
ref A deletion decref on node #3
This particular example isn't too bad since everyone is already done using
the object, but worse examples are possible, just harder to write down.

So, instead we use generational reference counting:
https://dl.acm.org/citation.cfm?id=74846
"""

import copy
import pickle

import attr
import asyncio

import taskloaf.serialize

class RefCount:
    def __init__(self):
        self.gen_counts = [1]

    def dec_ref(self, gen, n_children):
        n_gens = len(self.gen_counts)
        if n_gens < gen + 2:
            self.gen_counts.extend([0] * (gen + 2 - n_gens))
        self.gen_counts[gen] -= 1
        self.gen_counts[gen + 1] += n_children

    def alive(self):
        for c in self.gen_counts:
            if c != 0:
                return True
        return False

class RefManager:
    @attr.s
    class Entry:
        refcount = attr.ib()
        ptr = attr.ib()

    def __init__(self, worker):
        self.worker = worker
        self.entries = dict()
        self.worker.protocol.add_msg_type(
            'DECREF',
            type = DecRefMsg,
            handler = lambda worker, x: worker.ref_manager.dec_ref_owned(*x)
        )

    def dec_ref_owned(self, _id, gen, n_children):
        refcount = self.entries[_id].refcount
        refcount.dec_ref(gen, n_children)
        self.worker.log.debug(
            'decref', _id = _id, gen = gen, n_children = n_children,
            gen_count = refcount.gen_counts
        )
        if not refcount.alive():
            self.delete(_id)

    def delete(self, _id):
        self.worker.log.debug('delete', _id = _id)
        key = (self.worker.addr, _id)
        del self.worker.object_cache[key]
        self.worker.allocator.free(self.entries[_id].ptr)
        del self.entries[_id]

    def dec_ref(self, _id, gen, n_children, owner):
        if owner != self.worker.addr:
            self.worker.send(
                owner, self.worker.protocol.DECREF,
                (_id, gen, n_children)
            )
        else:
            self.dec_ref_owned(_id, gen, n_children)

    def new_ref(self, _id, ptr):
        self.worker.log.debug('new ref', _id = _id, ptr_start = ptr.start, ptr_block = ptr.block.idx)
        self.entries[_id] = RefManager.Entry(refcount = RefCount(), ptr = ptr)

class DecRefMsg:
    @staticmethod
    def serialize(args):
        m = taskloaf.message_capnp.Message.new_message()
        m.init('decRef')
        m.decRef.id = args[0]
        m.decRef.gen = args[1]
        m.decRef.nchildren = args[2]
        return m

    @staticmethod
    def deserialize(w, m):
        return m.decRef.id, m.decRef.gen, m.decRef.nchildren
