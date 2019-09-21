import numpy as np
import scipy.sparse

import taskloaf as tsk

import cppimport.import_hook  # noqa
import _sparse


def random_test_matrix(nrows, nnz):
    rows = np.random.randint(0, nrows, nnz).astype(np.int)
    cols = np.random.randint(0, nrows, nnz).astype(np.int)
    data = np.random.rand(nnz)
    A = scipy.sparse.coo_matrix(
        (data, (rows, cols)), shape=(nrows, nrows)
    ).tocsr()
    return A


class TskArray:
    def __init__(self, vals):
        assert len(vals.shape) == 1
        self.dtype = vals.dtype
        self.ref = tsk.alloc(vals.shape[0] * self.dtype.itemsize)
        self._array(self.ref.get_local())[:] = vals

    def _array(self, buf):
        return np.frombuffer(buf, dtype=self.dtype)

    async def array(self):
        return self._array(await self.ref.get())


class TskCSRChunk:
    def __init__(self, csr_mat):
        self.shape = csr_mat.shape
        self.indptr = TskArray(csr_mat.indptr)
        self.indices = TskArray(csr_mat.indices)
        self.data = TskArray(csr_mat.data)

    async def dot(self, v):
        assert v.shape[0] == self.shape[1]
        out = np.empty(self.shape[0])
        _sparse.csrmv_id(
            await self.indptr.array(),
            await self.indices.array(),
            await self.data.array(),
            v,
            out,
            False,
        )
        return out


async def distribute(mat, gang):
    n_chunks = len(gang)
    nrows = mat.shape[0]
    ncols = mat.shape[1]
    print(n_chunks, nrows, ncols)

    # TODO: optimal chunking?
    row_chunk_bounds = np.linspace(0, nrows, n_chunks + 1).astype(np.int)
    dist_chunks = []
    for i in range(n_chunks):
        start_row = row_chunk_bounds[i]
        end_row = row_chunk_bounds[i + 1]
        chunk_mat = mat[start_row:end_row]
        dist_chunks.append(TskCSRChunk(chunk_mat))

    return dist_chunks


async def dot(dist_chunks, v_ref, gang):
    jobs = []
    for i in range(len(dist_chunks)):

        async def dot_helper():
            v = await v_ref.get()
            return await dist_chunks[i].dot(v)

        jobs.append(tsk.task(dot_helper, to=gang[i]))

    out_chunks = []
    for i in range(len(dist_chunks)):
        out_chunks.append(await jobs[i])
    return np.concatenate(out_chunks)


def main():
    cfg = tsk.Cfg(n_workers=2)

    async def f():
        nrows = int(1e6)
        nnz = nrows * 10
        mat = random_test_matrix(nrows, nnz)
        vec = np.random.rand(nrows) - 0.5
        t = tsk.Timer()
        correct = mat.dot(vec)
        t.report("simple dot")

        print(np.sum(correct))

        gang = await tsk.ctx().wait_for_workers(cfg.n_workers)
        vec_ref = tsk.put(vec)

        async with tsk.Profiler(gang):
            dist_chunks = await distribute(mat, gang)

            t.restart()
            result = await dot(dist_chunks, vec_ref, gang)
            t.report("parallel dot")

        print(np.sum(result))

    tsk.zmq_run(cfg=cfg, f=f)


if __name__ == "__main__":
    main()
