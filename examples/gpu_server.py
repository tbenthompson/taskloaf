import numpy as np
import tectosaur.util.gpu as gpu
import taskloaf as tsk
arg = 1.0

def load_module():
    import os
    D = os.path.dirname(os.path.realpath(__file__))
    return gpu.load_gpu('kernels.cl', tmpl_dir = D, tmpl_args = dict(arg = arg))

async def gpu_run():
    # gd = tsk.get_service('gpu_data')
    # if 'add' not in gd:
    #     gd['add'] = (fnc, arg, gpu_R)
    # else:
    #     fnc, arg, gpu_R = gd['add']
    module = load_module()
    fnc = module.add
    R = np.random.rand(10000000)
    gpu_R = gpu.to_gpu(R)

    gpu_out = gpu.empty_gpu(gpu_R.shape)
    fnc(gpu_out, gpu_R, grid = (gpu_R.shape[0], 1, 1), block = (1, 1, 1))
    R2 = await gpu.get(gpu_out)
    gpu.logger.debug('run')

def setup_gpu_server(which_gpu):
    import os
    os.environ['CUDA_DEVICE'] = str(which_gpu)
    import taskloaf.worker as tsk_worker
    tsk_worker.services['gpu_data'] = dict()
    load_module()

async def submit():
    setup_prs = [
        tsk.task(lambda i=i: setup_gpu_server(i), to = i)
        for i in range(n_workers)
    ]
    for pr in setup_prs:
        await pr
    import time
    start = time.time()
    n_tasks = 8 * 2
    for j in range(10):
        prs = []
        for i in range(n_tasks):
            prs.append(tsk.task(gpu_run, to = i % n_workers))
        for i in range(n_tasks):
            await prs[i]
    print(time.time() - start)

n_workers = 8
tsk.cluster(n_workers, submit)

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
