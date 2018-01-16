import cppimport.import_hook
import taskloaf.cppworker

w = taskloaf.cppworker.Worker()
c = taskloaf.cppworker.MPIComm()
if c.addr == 0:
    mem = memoryview(bytearray(5))
    for i in range(5):
        mem[i] = i
    c.send(1, mem)
elif c.addr == 1:
    while True:
        b = c.recv()
        if b is not None:
            for i in range(5):
                print(b[i])
            break
    # c.recv()
