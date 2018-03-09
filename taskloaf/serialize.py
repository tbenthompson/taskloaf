import pickle
import cloudpickle
from io import BytesIO

import taskloaf.refcounting
import taskloaf.allocator

class CloudPicklerWithCtx(cloudpickle.CloudPickler):
    def __init__(self, worker, file):
        super().__init__(file)
        self.worker = worker
        self.refs = []

    def persistent_id(self, obj):
        if isinstance(obj, taskloaf.refcounting.Ref):
            self.refs.append(obj)
            return ('taskloaf_ref', len(self.refs) - 1)
        if isinstance(obj, taskloaf.allocator.MemoryBlock):
            return ('taskloaf_memory_block', self.worker.addr, obj.idx)
        return None
    persistent_id = persistent_id

class UnpicklerWithCtx(pickle.Unpickler):
    def __init__(self, worker, refs, file):
        super().__init__(file)
        self.worker = worker
        self.refs = refs

    def persistent_load(self, pid):
        type_tag = pid[0]
        if type_tag == 'taskloaf_ref':
            return self.refs[pid[1]]
        elif type_tag == 'taskloaf_memory_block':
            return taskloaf.allocator.deserialize_block(
                self.worker, pid[1], pid[2]
            )
        else:
            raise pickle.UnpicklingError("unsupported persistent object")

def dumps(worker, obj):
    with BytesIO() as file:
        cp = CloudPicklerWithCtx(worker, file)
        cp.dump(obj)
        return cp.refs, file.getvalue()

def loads(worker, refs, bytes_obj):
    with BytesIO(bytes_obj) as file:
        up = UnpicklerWithCtx(worker, refs, file)
        return up.load()
