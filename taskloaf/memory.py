# from taskloaf.worker import get_service, submit_task
from uuid import uuid1 as uuid

class Ref:
    def __init__(self, w, owner):
        self.w = w
        self.owner = owner
        self._id = uuid(self.owner)
        self.gen = 0
        self.n_children = 0

    def __getstate__(self):
        self.n_children += 1
        return dict(
            w = self.w,
            gen = self.gen + 1,
            n_children = 0,
            owner = self.owner,
            _id = self._id,
        )

    def __del__(self):
        data = (self.owner, self._id, self.gen, self.n_children)
        self.w.submit_task(self.owner, lambda w,d=data: w.memory.dec_ref(d))

    def get(self):
        return self.w.memory.get(self)

    def available(self):
        return self.w.memory.available(self)

class OwnedMemory:
    def __init__(self, value):
        self.value = value
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

class RemoteMemory:
    def __init__(self, value):
        self.value = value

class MemoryManager:
    def __init__(self, w):
        self.blocks = dict()
        self.w = w

    def put(self, v, r = None):
        if r is None:
            r = Ref(self.w, self.w.comm.addr)
            self.blocks[r._id] = OwnedMemory(v)
        elif r.owner == self.w.comm.addr:
            self.blocks[r._id] = OwnedMemory(v)
        else:
            self.blocks[r._id] = RemoteMemory(v)
        return r

    def get(self, r):
        return self.blocks[r._id].value

    def available(self, r):
        return r._id in self.blocks

    def delete(self, r):
        del self.blocks[r._id]

    def dec_ref(self, d):
        owner, _id, gen, n_children = d
        mem = self.blocks[_id]
        mem.dec_ref(gen, n_children)
        if not mem.alive():
            del self.blocks[_id]

    def n_entries(self):
        return len(self.blocks)
