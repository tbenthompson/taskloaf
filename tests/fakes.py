from taskloaf.promise import Promise
from taskloaf.memory import Ref
from taskloaf.worker import Worker
from taskloaf.run import add_plugins

class FakeWorker(Worker):
    @property
    def addr(self):
        return 13

    def send(self, to, type_code, objs):
        pass

class FakePromise(Promise):
    def __init__(self, w, owner):
        self.r = Ref(w, owner)

def fake_worker():
    w = FakeWorker()
    add_plugins(w)
    return w
