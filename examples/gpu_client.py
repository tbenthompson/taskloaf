import taskloaf as tsk

gpu_addr = 0

async def work_builder():
    print("YO!")
    addr = tsk.get_service('comm').addr
    def add_task():
        print("PEACE" + str(addr))
    await tsk.task(add_task, to = gpu_addr)
    return 2.0

async def f2(x):
    return x * 2

async def submit():
    pr1 = tsk.task(work_builder, to = 0).then(f2)
    pr2 = tsk.task(work_builder, to = 1)
    print(await pr1)
    print(await pr2)
    tsk.task(tsk.shutdown, to = 0)
    tsk.task(tsk.shutdown, to = 1)

tsk.cluster(2, submit)
