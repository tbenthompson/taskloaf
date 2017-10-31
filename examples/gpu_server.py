import numpy as np
import tectosaur.util.gpu as gpu
import taskloaf as tsk

code = """
${cluda_preamble}

KERNEL
void add(GLOBAL_MEM float* result, GLOBAL_MEM float* in) {
    int i = get_global_id(0);
    result[i] = in[i] + ${arg};
}
"""
arg = 1.0
module = gpu.load_gpu_from_code(code, tmpl_args = dict(arg = arg))
fnc = module.add

#
# R = np.random.rand(10)
# gpu_R = gpu.to_gpu(R)
# gpu_out = gpu.empty_gpu(R.shape)
# fnc(gpu_out, gpu_R, grid = (R.shape[0], 1, 1), block = (1, 1, 1))
# R2 = tsk.run(gpu.get(gpu_out))
# np.testing.assert_almost_equal(R2, R + arg)

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
