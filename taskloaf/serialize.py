import pickle
import cloudpickle
from io import BytesIO

import taskloaf.worker

marker = 'taskloaf.worker'
class CloudPicklerWithCtx(cloudpickle.CloudPickler):
    def persistent_id(self, obj):
        if isinstance(obj, taskloaf.worker.Worker):
            return marker
        return None

class PicklerWithCtx(pickle.Pickler):
    def persistent_id(self, obj):
        if isinstance(obj, taskloaf.worker.Worker):
            return marker
        return None

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
    with BytesIO() as file:
        cp = CloudPicklerWithCtx(file)
        cp.dump(obj)
        return file.getvalue()

def loads(w, bytes_obj):
    with BytesIO(bytes_obj) as file:
        up = UnpicklerWithCtx(w, file)
        return up.load()

