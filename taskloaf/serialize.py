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
        # if isinstance(obj, taskloaf.ref.GCRef):
        #     return marker
        return None
    persistent_id = persistent_id

class UnpicklerWithCtx(pickle.Unpickler):
    def __init__(self, w, file):
        super().__init__(file)
        self.w = w

    def persistent_load(self, pid):
        type_tag = pid
        if type_tag == marker:
            return self.w
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
