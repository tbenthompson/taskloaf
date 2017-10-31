import taskloaf as tsk

gpu_addr = 0

async def work_builder():
    print("YO!")
    addr = tsk.get_service('comm').addr
    def add_task():
        print("PEACE" + str(addr))
        return addr
    rem_addr = await tsk.task(add_task, to = gpu_addr)
    return (2.0, rem_addr)

def f2(x):
    return x * 2

async def submit():
    pr1 = tsk.task(work_builder, to = 0).then(f2)
    pr2 = tsk.task(work_builder, to = 1)
    print(await pr1)
    print(await pr2)
    return 5.0

print(tsk.cluster(2, submit))
