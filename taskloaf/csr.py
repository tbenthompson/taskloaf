import asyncio

import numpy as np

import taskloaf as tsk

import cppimport.import_hook  # noqa
import _sparse


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
        indptr_array, indices_array, data_array = await asyncio.gather(
            self.indptr.array(), self.indices.array(), self.data.array()
        )
        _sparse.csrmv_id(
            indptr_array, indices_array, data_array, v, out, False
        )
        return None


class TskCSR:
    def __init__(self, chunks, gang):
        self.chunks = chunks
        self.gang = gang
        self._setup_dot()

    def _setup_dot(self):
        self.dot_fnc_refs = []
        for i in range(len(self.chunks)):

            this_chunk = self.chunks[i]

            async def dot_helper(v_ref):
                v = await v_ref.get()
                return await this_chunk.dot(v)

            self.dot_fnc_refs.append(tsk.put(dot_helper))

    async def dot(self, v_promise):
        jobs = []
        for i in range(len(self.chunks)):
            jobs.append(v_promise.then(self.dot_fnc_refs[i], to=self.gang[i]))

        out_chunks = []
        for i in range(len(jobs)):
            out_chunks.append(await jobs[i])
        return None
        # return np.concatenate(out_chunks)


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
        dist_chunks.append(TskCSRChunk(chunk_mat))

    return TskCSR(dist_chunks, gang)
