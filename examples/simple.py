import taskloaf as tsk

async def submit():
    addr = tsk.get_service('comm').addr
    def print_then_shutdown():
        print('hI from proc ' + str(addr))
        tsk.submit_task(0, tsk.shutdown)
    tsk.submit_task(0, print_then_shutdown)

if __name__ == "__main__":
    tsk.cluster(1, submit)
    tsk.cluster(1, submit, runner = tsk.mpirun)
