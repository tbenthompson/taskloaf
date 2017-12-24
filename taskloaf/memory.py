import capnp
import taskloaf.task_capnp

class Ref:
    def __init__(self, w, owner):
        self.w = w
        self.owner = owner
        self.creator = w.addr
        self._id = w.get_id()
        self.gen = 0
        self.n_children = 0

    def index(self):
        return (self.creator, self._id)

    def __getstate__(self):
        self.n_children += 1
        return dict(
            w = self.w,
            owner = self.owner,
            creator = self.creator,
            _id = self._id,
            gen = self.gen + 1,
            n_children = 0,
        )

    def __del__(self):
        self.w.send(
            self.owner, self.w.protocol.DECREF,
            (self.creator, self._id, self.gen, self.n_children)
        )

    def put(self, v):
        return self.w.memory.put(v, r = self)

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

def decref_encoder(creator, _id, gen, n_children):
    m = taskloaf.task_capnp.Message.new_message()
    m.init('decref')
    m.decref.creator = creator
    m.decref.id = _id
    m.decref.gen = gen
    m.decref.nchildren = n_children
    return bytes(8) + m.to_bytes()

def decref_decoder(w, b):
    decref_obj = taskloaf.task_capnp.Message.from_bytes(b).decref
    return decref_obj.creator, decref_obj.id, decref_obj.gen, decref_obj.nchildren

class MemoryManager:
    def __init__(self, w):
        self.blocks = dict()
        self.w = w
        self.w.protocol.add_handler(
            'DECREF',
            encoder = decref_encoder,
            decoder = decref_decoder,
            work_builder = lambda x: lambda w: w.memory.dec_ref(*x)
        )

    def put(self, v, r = None):
        if r is None:
            r = Ref(self.w, self.w.addr)
            self.blocks[r.index()] = OwnedMemory(v)
        elif r.owner == self.w.addr:
            self.blocks[r.index()] = OwnedMemory(v)
        else:
            self.blocks[r.index()] = RemoteMemory(v)
        return r

    def get(self, r):
        return self.blocks[r.index()].value

    def available(self, r):
        return r.index() in self.blocks

    def delete(self, r):
        del self.blocks[r.index()]

    def dec_ref(self, creator, _id, gen, n_children):
        idx = (creator, _id)
        mem = self.blocks[idx]
        mem.dec_ref(gen, n_children)
        if not mem.alive():
            del self.blocks[idx]

    def n_entries(self):
        return len(self.blocks)
