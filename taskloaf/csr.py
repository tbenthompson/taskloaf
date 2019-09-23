import asyncio

import numpy as np

import taskloaf as tsk

import cppimport.import_hook  # noqa
import _sparse


class TskArray:
    def __init__(self, vals=None, size=None):
        if vals is not None:
            assert len(vals.shape) == 1
            self.dtype = vals.dtype
            self.ref = tsk.alloc(vals.shape[0] * self.dtype.itemsize)
            self._array(self.ref.get_local())[:] = vals
        else:
            self.dtype = np.float64()
            self.ref = tsk.alloc(size * self.dtype.itemsize)

    def _array(self, buf):
        return np.frombuffer(buf, dtype=self.dtype)

    async def array(self):
        return self._array(await self.ref.get())


class TskCSRChunk:
    def __init__(self, csr_mat, start_row):
        self.shape = csr_mat.shape
        self.start_row = start_row
        self.end_row = self.start_row + self.shape[0]
        self.indptr = TskArray(vals=csr_mat.indptr)
        self.indices = TskArray(vals=csr_mat.indices)
        self.data = TskArray(vals=csr_mat.data)

    async def dot(self, v, tsk_out):
        assert v.shape[0] == self.shape[1]
        indptr_arr, indices_arr, data_arr, out_arr = await asyncio.gather(
            self.indptr.array(),
            self.indices.array(),
            self.data.array(),
            tsk_out.array(),
        )
        out_chunk = out_arr[self.start_row : self.end_row]
        _sparse.csrmv_id(
            indptr_arr, indices_arr, data_arr, v, out_chunk, False
        )
        return tsk_out


class TskCSR:
    def __init__(self, shape, chunks, gang):
        self.shape = shape
        self.chunks = chunks
        self.gang = gang
        self._setup_dot()

    def _setup_dot(self):
        self.dot_fnc_refs = []
        for i in range(len(self.chunks)):

            async def dot_helper(v, tsk_out, this_chunk=self.chunks[i]):
                v = await v.array()
                await this_chunk.dot(v, tsk_out)

            self.dot_fnc_refs.append(tsk.put(dot_helper))

    async def dot(self, v_ref):
        out = TskArray(size=self.shape[0])
        jobs = []
        for i in range(len(self.chunks)):
            jobs.append(
                tsk.task(self.dot_fnc_refs[i], v_ref, out, to=self.gang[i])
            )

        for i in range(len(jobs)):
            await jobs[i]
        return await out.array()
        # return np.concatenate([await c.array() for c in out_chunks])


def distribute(mat, gang):
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
        dist_chunks.append(TskCSRChunk(chunk_mat, start_row))

    return TskCSR(mat.shape, dist_chunks, gang)
