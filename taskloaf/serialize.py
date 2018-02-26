import pickle
import cloudpickle
from io import BytesIO

import taskloaf.ref

marker = 'taskloaf.worker'

class BlobWithRefs:
    def __init__(self):
        self.refs = []
        self.blob = None

class CloudPicklerWithCtx(cloudpickle.CloudPickler):
    def persistent_id(self, obj):
        if isinstance(obj, taskloaf.ref.Ref):
            obj = obj.convert()
        if isinstance(obj, taskloaf.ref.GCRef):
            #TODO: move to ref.py
            obj.n_children += 1
            return ('taskloaf_ref', (obj.owner, obj._id, obj.gen + 1, obj.deserialize, obj.ptr.start, obj.ptr.end, obj.ptr.block.idx))
        return None
    persistent_id = persistent_id

class UnpicklerWithCtx(pickle.Unpickler):
    def __init__(self, worker, file):
        super().__init__(file)
        self.worker = worker

    def persistent_load(self, pid):
        type_tag, data = pid
        if type_tag == 'taskloaf_ref':
            #TODO: move to ref.py
            ref = taskloaf.ref.GCRef.__new__(taskloaf.ref.GCRef)
            ref.owner = data[0]
            ref._id = data[1]
            ref.gen = data[2]
            ref.deserialize = data[3]
            ref.n_children = 0
            ref.worker = self.worker
            ref.ptr = taskloaf.allocator.Ptr(
                start = data[4],
                end = data[5],
                block = self.worker.remote_shmem.get_block(ref.owner, data[6])
            )
            return ref
        else:
            raise pickle.UnpicklingError("unsupported persistent object")

def dumps(obj):
    # out = MsgWithRefs()
    with BytesIO() as file:
        # cp = CloudPicklerWithCtx(out, file)
        cp = CloudPicklerWithCtx(file)
        cp.dump(obj)
        return file.getvalue()
        # out.blob = file.getvalue()
    # return out

def loads(w, bytes_obj):
    with BytesIO(bytes_obj) as file:
        up = UnpicklerWithCtx(w, file)
        return up.load()
