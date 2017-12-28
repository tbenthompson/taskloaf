from taskloaf.promise import Promise
from taskloaf.memory import DistributedRef
from taskloaf.worker import Worker, NullComm
from taskloaf.run import add_plugins

class FakeWorker(Worker):
    def __init__(self):
        self.sends = []
        super().__init__(NullComm())

    @property
    def addr(self):
        return 13

    def run_work(self, f, *args):
        pass

    def send(self, to, type_code, objs):
        self.sends.append((to, type_code, objs))

class FakePromise(Promise):
    def __init__(self, w, owner):
        self.dref = DistributedRef(w, owner)

def fake_worker():
    w = FakeWorker()
    add_plugins(w)
    return w
