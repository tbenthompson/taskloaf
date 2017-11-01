import numpy as np
import tectosaur.util.gpu as gpu
import taskloaf as tsk

async def gpu_run():
    # gd = tsk.get_service('gpu_data')
    # if 'add' not in gd:
    #     gd['add'] = (fnc, arg, gpu_R)
    # else:
    #     fnc, arg, gpu_R = gd['add']
    arg = 1.0
    import os
    D = os.path.dirname(os.path.realpath(__file__))
    module = gpu.load_gpu('kernels.cl', tmpl_dir = D, tmpl_args = dict(arg = arg))
    fnc = module.add
    R = np.random.rand(100000000)
    gpu_R = gpu.to_gpu(R)

    gpu_out = gpu.empty_gpu(gpu_R.shape)
    fnc(gpu_out, gpu_R, grid = (gpu_R.shape[0], 1, 1), block = (1, 1, 1))
    R2 = await gpu.get(gpu_out)
    gpu.logger.debug('run')

gpu_addr = 0

async def setup_gpu_server():
    import taskloaf.worker as tsk_worker
    tsk_worker.services['gpu_data'] = dict()
    await gpu_run()

async def submit():
    await tsk.task(setup_gpu_server, to = 0)
    await tsk.task(setup_gpu_server, to = 1)
    import time
    start = time.time()
    n_tasks = 10
    prs = []
    for i in range(n_tasks):
        prs.append(tsk.task(gpu_run, to = 1 if i < 5 else 0))
    for i in range(n_tasks):
        await prs[i]
    print(time.time() - start)

tsk.cluster(4, submit)


#
# async def work_builder():
#     print("YO!")
#     addr = tsk.get_service('comm').addr
#     def add_task():
#         print("PEACE" + str(addr))
#         return addr
#     rem_addr = await tsk.task(add_task, to = gpu_addr)
#     return (2.0, rem_addr)
#
# def f2(x):
#     return x * 2

    # pr1 = tsk.task(work_builder, to = 0).then(f2)
    # pr2 = tsk.task(work_builder, to = 1)
    # print(await pr1)
    # print(await pr2)
    # return 5.0
