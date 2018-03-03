import pickle
import cloudpickle
from io import BytesIO

import taskloaf.ref

marker = 'taskloaf.worker'

class CloudPicklerWithCtx(cloudpickle.CloudPickler):
    def __init__(self, file):
        super().__init__(file)
        self.refs = []

    def persistent_id(self, obj):
        if isinstance(obj, taskloaf.ref.Ref):
            obj = obj.convert()
        if isinstance(obj, taskloaf.ref.GCRef):
            self.refs.append(obj)
            return ('taskloaf_ref', len(self.refs) - 1)
        return None
    persistent_id = persistent_id

class UnpicklerWithCtx(pickle.Unpickler):
    def __init__(self, worker, refs, file):
        super().__init__(file)
        self.refs = refs
        self.worker = worker

    def persistent_load(self, pid):
        type_tag, ref_idx = pid
        if type_tag == 'taskloaf_ref':
            return self.refs[ref_idx]
        else:
            raise pickle.UnpicklingError("unsupported persistent object")

def dumps(obj):
    with BytesIO() as file:
        cp = CloudPicklerWithCtx(file)
        cp.dump(obj)
        return cp.refs, file.getvalue()

def loads(w, refs, bytes_obj):
    with BytesIO(bytes_obj) as file:
        up = UnpicklerWithCtx(w, refs, file)
        return up.load()
