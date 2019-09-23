import logging

import numpy as np
import scipy.sparse

import taskloaf as tsk

from taskloaf.csr import distribute, TskArray


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
        nrows = int(1e7)
        nnz = nrows * 10
        n_repeats = 1
        mat = random_test_matrix(nrows, nnz)
        vec = np.random.rand(nrows) - 0.5
        t = tsk.Timer()
        for i in range(n_repeats):
            correct = mat.dot(vec)
        t.report("simple dot")

        gang = await tsk.ctx().wait_for_workers(cfg.n_workers)
        t.report("wait for workers")

        t.report("launch profiler")
        tsk_vec = TskArray(vals=vec)
        t.report("shmem v")

        tsk_mat = distribute(mat, gang)
        t.report("distribute mat")

        result = await tsk_mat.dot(tsk_vec)
        t.report("first dot")

        async with tsk.Profiler(gang):
            t.restart()
            for i in range(n_repeats):
                result = await tsk_mat.dot(tsk_vec)
            t.report("parallel dot")

        print(np.sum(correct))
        print(np.sum(result))
        assert np.sum(result) == np.sum(correct)

    tsk.zmq_run(cfg=cfg, f=f)


if __name__ == "__main__":
    main()
