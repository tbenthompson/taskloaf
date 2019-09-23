import logging

import numpy as np
import scipy.sparse

import taskloaf as tsk

from taskloaf.csr import distribute


def random_test_matrix(nrows, nnz):
    rows = np.random.randint(0, nrows, nnz).astype(np.int)
    cols = np.random.randint(0, nrows, nnz).astype(np.int)
    data = np.random.rand(nnz)
    A = scipy.sparse.coo_matrix(
        (data, (rows, cols)), shape=(nrows, nrows)
    ).tocsr()
    return A


def setup_worker(name, cfg):
    tsk.cfg.stdout_logging(name, logger_name="taskloaf.profile")


def main():
    cfg = tsk.Cfg(
        n_workers=2, log_level=logging.WARN, initializer=setup_worker
    )

    async def f():
        nrows = int(1e6)
        nnz = nrows * 10
        n_repeats = 10
        mat = random_test_matrix(nrows, nnz)
        vec = np.random.rand(nrows) - 0.5
        t = tsk.Timer()
        for i in range(n_repeats):
            correct = mat.dot(vec)
        t.report("simple dot")

        print(np.sum(correct))

        gang = await tsk.ctx().wait_for_workers(cfg.n_workers)
        vec_ref = tsk.put(vec)
        vec_promise = tsk.task(lambda: vec_ref)
        print("SUM1: ", np.sum(vec))

        async def await_sum(x):
            print(x)
            print(type(x))
            return np.sum(await x.get())

        print("SUM2: ", await (vec_promise.then(await_sum)))
        return

        async with tsk.Profiler(gang):
            tsk_mat = distribute(mat, gang)

            t.restart()
            for i in range(n_repeats):
                result = await tsk_mat.dot(vec_promise)
            t.report("parallel dot")

        print(np.sum(result))

    tsk.zmq_run(cfg=cfg, f=f)


if __name__ == "__main__":
    main()
