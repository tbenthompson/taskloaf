"""
Causality is an ugly problem for distributed incref/decref systems.
So, instead we use generational reference counting:
https://dl.acm.org/citation.cfm?id=74846

This also reduces the number of network communications necessary by
approximately half.

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
"""

import copy
import pickle

import attr
import asyncio

import taskloaf.serialize

def setup_plugin(worker):
    worker.ref_manager = RefManager(worker)

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
        on_delete = attr.ib()

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
        self.entries[_id].on_delete(_id)
        del self.entries[_id]

    def dec_ref(self, _id, gen, n_children, owner):
        if owner != self.worker.addr:
            self.worker.send(
                owner, self.worker.protocol.DECREF,
                (_id, gen, n_children)
            )
        else:
            self.dec_ref_owned(_id, gen, n_children)

    def new_ref(self, _id, on_delete):
        self.entries[_id] = RefManager.Entry(
            refcount = RefCount(),
            on_delete = on_delete
        )

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

class RefCopyException(Exception):
    pass

class Ref:
    def __init__(self, worker, on_delete, child_refs = None, _id = None):
        self.worker = worker
        if child_refs is None:
            child_refs = []
        self.child_refs = child_refs
        self.owner = worker.addr
        if _id == None:
            _id = self.worker.get_new_id()
        self._id = _id
        self.gen = 0
        self.n_children = 0
        self.log('new ref')
        self.worker.ref_manager.new_ref(_id, on_delete)

    def __getstate__(self):
        raise RefCopyException()

    def key(self):
        return (self.owner, self._id)

    def log(self, text):
        self.worker.log.debug(
            text, _id = self._id, n_children = self.n_children,
            gen = self.gen, owner = self.owner
        )

    def __del__(self):
        self.log('del ref')
        self.worker.ref_manager.dec_ref(
            self._id, self.gen, self.n_children,
            owner = self.owner
        )

    def encode_reflist(self, msg):
        msg.init('refList', len(self.child_refs))
        if isinstance(self.child_refs, list):
            for i in range(len(self.child_refs)):
                self.child_refs[i].encode_capnp(msg.refList[i])
        else:
            for i, r in enumerate(self.child_refs):
                msg.refList[i] = r

    def _ensure_child_refs_deserialized(self):
        if isinstance(self.child_refs, list):
            return
        self.child_refs = [
            Ref.decode_capnp(self.worker, child_ref, child = True)
            for child_ref in self.child_refs
        ]

    # After encoding or pickling, the __del__ call will only happen if
    # the object is decoded properly and nothing bad happens. This is scary,
    # but is hard to avoid without adding a whole lot more synchronization and
    # tracking. Using the log statements, checking whether encodes and decodes
    # are paired should be possible.
    def encode_capnp(self, msg):
        self.log('encode ref')
        self.n_children += 1
        msg.owner = self.owner
        msg.id = self._id
        msg.gen = self.gen + 1
        self.encode_reflist(msg)

    @classmethod
    def decode_capnp(cls, worker, msg, child = False):
        ref = Ref.__new__(Ref)
        ref.worker = worker
        ref.owner = msg.owner
        ref._id = msg.id
        ref.gen = msg.gen
        ref.n_children = 0
        ref.log('decode ref')

        ref.child_refs = msg.refList
        if not child:
            ref._ensure_child_refs_deserialized()

        return ref
