from taskloaf.protocol import Protocol

def test_roundtrip():
    p = Protocol()
    p.add_handler('simple')
    data = (123, 456)
    b = p.encode(p.simple, *data)
    out = p.decode(None, b)
    assert(out == (p.simple, data))

def test_work():
    p = Protocol()
    p.add_handler('simple')
    assert(p.get_name(p.simple) == 'simple')

    def set():
        set.val = 1
    set.val = 0
    p.build_work(0, [set])()
    assert(set.val == 1)
