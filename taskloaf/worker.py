comm = None
run = None

def launch(pcomm):
    global comm, run
    comm = pcomm
    run = True

    while run:
        data = comm.recv()
        if data is not None:
            data()
