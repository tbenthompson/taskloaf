import pickle
import cloudpickle
from io import BytesIO

import taskloaf.refcounting
import taskloaf.allocator


class CloudPicklerWithCtx(cloudpickle.CloudPickler):
    def __init__(self, file):
        super().__init__(file)
        self.refs = []

    def persistent_id(self, obj):
        if isinstance(obj, taskloaf.refcounting.Ref):
            self.refs.append(obj)
            return ("taskloaf_ref", len(self.refs) - 1)
        if isinstance(obj, taskloaf.allocator.MemoryBlock):
            return ("taskloaf_memory_block", taskloaf.ctx().name, obj.idx)
        return None

    persistent_id = persistent_id


class UnpicklerWithCtx(pickle.Unpickler):
    def __init__(self, refs, file):
        super().__init__(file)
        self.refs = refs

    def persistent_load(self, pid):
        type_tag = pid[0]
        if type_tag == "taskloaf_ref":
            return self.refs[pid[1]]
        elif type_tag == "taskloaf_memory_block":
            return taskloaf.allocator.deserialize_block(pid[1], pid[2])
        else:
            raise pickle.UnpicklingError("unsupported persistent object")


def dumps(obj):
    with BytesIO() as file:
        cp = CloudPicklerWithCtx(file)
        cp.dump(obj)
        return cp.refs, file.getvalue()


def loads(refs, bytes_obj):
    with BytesIO(bytes_obj) as file:
        up = UnpicklerWithCtx(refs, file)
        return up.load()
