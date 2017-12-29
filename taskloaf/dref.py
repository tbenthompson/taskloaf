class DistributedRef:
    def __init__(self, worker, owner):
        self.worker = worker
        self.owner = owner
        self.creator = worker.addr
        self._id = worker.memory.get_new_id()
        self.gen = 0
        self.n_children = 0
        self.shmem_ptr = 0

    def index(self):
        return (self.creator, self._id)

    def __getstate__(self):
        self.n_children += 1
        return dict(
            worker = self.worker,
            owner = self.owner,
            creator = self.creator,
            _id = self._id,
            gen = self.gen + 1,
            n_children = 0,
            shmem_ptr = 0
        )

    def __del__(self):
        self.worker.memory.dec_ref(
            self.creator, self._id, self.gen, self.n_children,
            owner = self.owner
        )

    # TODO: After encoding or pickling, the __del__ call will only happen if
    # the object is decoded properly and nothing bad happens. This is scary,
    # but is hard to avoid without adding a whole lot more synchronization and
    # tracking. It would be worth adding some tools to check that memory isn't
    # being leaked. Maybe a counter of number of encoded and decoded drefs
    # (those #s should be equal).
    def encode_capnp(self, dest):
        self.n_children += 1
        dest.owner = self.owner
        dest.creator = self.creator
        dest.id = self._id
        dest.gen = self.gen + 1
        dest.shmemPtr = self.shmem_ptr

    @classmethod
    def decode_capnp(cls, worker, m):
        dref = DistributedRef.__new__(DistributedRef)
        dref.owner = m.owner
        dref.creator = m.creator
        dref._id = m.id
        dref.gen = m.gen
        dref.shmem_ptr = m.shmemPtr
        dref.n_children = 0
        dref.worker = worker
        return dref
