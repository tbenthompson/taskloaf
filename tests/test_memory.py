from taskloaf.memory import *
from fakes import fake_worker

def test_decref_encode():
    w = fake_worker()
    coded = decref_encoder(1, 2, 3, 4)
    creator, _id, gen, n_children = decref_decoder(w, memoryview(coded)[8:])
    assert(creator == 1)
    assert(_id == 2)
    assert(gen == 3)
    assert(n_children == 4)
