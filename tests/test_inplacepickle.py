import pickle

# Just a quick experiment pickling into an already existing memory buffer
class InPlaceByteWriter:
    def __init__(self, memory, loc = 0):
        self.memory = memory
        self.loc = loc

    def write(self, bytes):
        n_bytes = len(bytes)
        next_loc = self.loc + n_bytes
        self.memory[self.loc:next_loc] = bytes
        self.loc = next_loc

def test_inplace():
    m = bytearray(13)
    bw = InPlaceByteWriter(memoryview(m))
    pickle.dump('abc', bw)
    assert(m == pickle.dumps('abc'))
