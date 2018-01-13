import time
import asyncio
import numpy as np
import scipy.sparse
from mpi4py import MPI

import taskloaf as tsk

import cppimport.import_hook
import _sparse

scipy.sparse.coo_matrix._check = lambda self: None

n_cores = MPI.COMM_WORLD.Get_size()

def make_test_matrix(nrows, nnzperrow):
    # indptr = np.arange(nrows + 1) * nnzperrow
    # indices = np.arange(nrows)
    # data = np.ones(nrows)
    # A = scipy.sparse.csr_matrix((data, indices, indptr), shape = (nrows, nrows))
    indptr = np.arange(nrows + 1) * nnzperrow
    indices = np.random.randint(0, nrows, nrows * nnzperrow)
    data = np.ones(nrows * nnzperrow)
    A = scipy.sparse.csr_matrix((data, indices, indptr), shape = (nrows, nrows))
    # rows = np.random.randint(0, nrows, nnz).astype(np.int32)
    # cols = np.random.randint(0, nrows, nnz).astype(np.int32)
    # data = np.random.rand(nnz)
    # A = scipy.sparse.coo_matrix((data, (rows, cols)), shape = (nrows, nrows)).tocsr()
    return A

def put_fnc(w, f):
    f_bytes = tsk.serialize.dumps(f)
    assert(len(f_bytes) < 5e4)
    return tsk.put(w,serialized = f_bytes)

# def distorted_linspace(a, b, n):
#     split = int(np.floor((b - a) * 0.2)) + a
#     return np.array([0,split] + np.linspace(split, b, n - 1)[1:].tolist())

async def map_helper(w, f, N_low, N_high, core_range, *args):
    t = tsk.Timer(None)
    n_chunks = len(core_range)
    chunk_bounds = np.linspace(N_low, N_high, n_chunks + 1).astype(np.int32)
    out_chunks = [
        tsk.task(
            w, f,
            [chunk_bounds[i], chunk_bounds[i + 1]] + list(args),
            to = core_range[i]
        )
        for i in range(n_chunks)
    ]
    t.report('submit map')
    results = []
    for c in out_chunks:
        results.append(await c)
    t.report('wait map')
    return results

async def super_chunk(w, args):
    s, e, n_super_chunks, f_dref, n_inner, remaining_args = args
    # print('super took', time.time() - remaining_args[0], 'to launch')
    start_row, end_row = np.linspace(0, n_inner, n_super_chunks + 1)[s:(e+1)]
    start_core, end_core = np.linspace(0, n_cores, n_super_chunks + 1).astype(np.int32)[s:(e+1)]
    return await map_helper(
        w, f_dref, start_row, end_row, range(start_core, end_core),
        *remaining_args
    )

def flatten(l):
    return [item for sublist in l for item in sublist]

async def map(w, f_dref, N, *args, n_super_chunks = 8):
    if not hasattr(w, 'super_dref'):
        w.super_dref = put_fnc(w, super_chunk)
    if n_super_chunks == 1:
        return await super_chunk(w, [0, 1, 1, f_dref, N] + [list(args)])
    return flatten(await map_helper(
        w, w.super_dref, 0, n_super_chunks, range(n_super_chunks),
        n_super_chunks, f_dref, N, args
    ))

async def submit(w):
    t = tsk.Timer()
    nrows = int(1e8)
    # nnz = 5 * nrows

    A = make_test_matrix(nrows, 1)
    t.report('build csr')
    v = np.random.rand(nrows)
    t.report('gen v')
    correct = v.copy() #np.empty(A.shape[0])
    for i in range(100):
        v[:] = correct
        t.report('copy')
        _sparse.csrmv(A.indptr, A.indices, A.data, v, correct, True)
        t.report('csrmv')

    data_dref = tsk.put(w, value = A.data.data.cast('B'), eager_alloc = 1)
    indptr_dref = tsk.put(w, value = A.indptr.data.cast('B'), eager_alloc = 1)
    indices_dref = tsk.put(w, value = A.indices.data.cast('B'), eager_alloc = 1)
    v_dref = tsk.put(w, value = v.data.cast('B'), eager_alloc = 1)
    t.report('put matrix')

    print('total bytes', A.data.nbytes + A.indptr.nbytes + A.indices.nbytes + v.nbytes * 2)


    async def dot_chunk(w, args):
        t = tsk.Timer(None)

        start_row, end_row, out_dref = args
        # print(start_row, end_row)

        v_buf = await tsk.remote_get(w, v_dref)
        out_buf = await tsk.remote_get(w, out_dref)
        data_buf = await tsk.remote_get(w, data_dref)
        indptr_buf = await tsk.remote_get(w, indptr_dref)
        indices_buf = await tsk.remote_get(w, indices_dref)

        v = np.frombuffer(v_buf, dtype = np.float64)
        indptr = np.frombuffer(indptr_buf, dtype = np.int32)[start_row:end_row+1]
        indices = np.frombuffer(indices_buf, dtype = np.int32)
        data = np.frombuffer(data_buf, dtype = np.float64)
        out = np.frombuffer(out_buf, dtype = np.float64)

        t.report(str(w.addr) + ' setup')

        inner_chunk_size = int(1e9)
        for i in range(0, indptr.shape[0], inner_chunk_size):
            # print(start_row, i, i+inner_chunk_size)
            _sparse.csrmv(
                indptr[i:(i + inner_chunk_size + 1)],
                indices,
                data, v, out[(start_row+i):(start_row+i+inner_chunk_size)],
                False
            )
            await asyncio.sleep(0)
        t.report(str(w.addr) + ' dot')

    dot_chunk_dref = put_fnc(w, dot_chunk)
    t.report('put fnc')

    out_dref = tsk.alloc(w, nrows * 8)
    out = np.frombuffer(w.memory.get_local(out_dref), dtype = np.float64)
    t.report('alloc out')

    n_super_chunks = int(np.floor(np.sqrt(n_cores)))
    async with tsk.Profiler(w, range(1)):
        for i in range(50):
            await map(w, dot_chunk_dref, nrows, out_dref, n_super_chunks = n_super_chunks)
            t.report('dot')

    # np.testing.assert_almost_equal(out, correct)
    # t.report('check')

async def submit2(w):
    t = tsk.Timer()
    nrows = int(5e7)
    n_super_chunks = int(np.floor(np.sqrt(n_cores)))

    # A = make_test_matrix(nrows, 1)
    # t.report('build csr')
    # v = np.random.rand(nrows)
    # t.report('gen v')
    # correct = np.empty(A.shape[0])
    # _sparse.csrmv(A.indptr, A.indices, A.data, v, correct, True)
    # t.report('serial1')

    # v_dref = w.memory.put(value = v.data.cast('B'), eager_alloc = 1)
    # data_dref = w.memory.put(value = A.data.data.cast('B'), eager_alloc = 1)
    # indptr_dref = w.memory.put(value = A.indptr.data.cast('B'), eager_alloc = 1)
    # indices_dref = w.memory.put(value = A.indices.data.cast('B'), eager_alloc = 1)
    # t.report('put matrix')

    # async def build_local_matrix(w, args):
    #     t = tsk.Timer(output_fnc = lambda x: None)
    #     start_row, end_row = args

    #     data_buf = await tsk.remote_get(w, data_dref)
    #     indptr_buf = await tsk.remote_get(w, indptr_dref)
    #     indices_buf = await tsk.remote_get(w, indices_dref)

    #     indptr = np.frombuffer(indptr_buf, dtype = np.int32)[start_row:end_row+1].copy()
    #     indices = np.frombuffer(indices_buf, dtype = np.int32)[indptr[0]:indptr[-1]].copy()
    #     data = np.frombuffer(data_buf, dtype = np.float64)[indptr[0]:indptr[-1]].copy()
    #     indptr -= indptr[0]

    #     out = np.empty(end_row - start_row)

    #     matrix = (indptr, indices, data, out)
    #     matrix_dref = tsk.put(w, value = matrix)

    #     v_buf = await tsk.remote_get(w, v_dref)
    #     v = np.frombuffer(v_buf, dtype = np.float64).copy()
    #     v_dref_out = tsk.put(w, value = v)
    #     t.report(str(w.addr) + ' distribute')

    #     return (v_dref_out, matrix_dref)
    # build_dref = put_fnc(w, build_local_matrix)

    # def rand_vec(w, args):
    #     start_row, end_row = args
    #     v_dref = tsk.put(w, value = np.random.rand(end_row - start_row).data.cast('B'), eager_alloc = 1)
    #     return v_dref
    # v_chunks = await map(w, rand_vec, nrows, n_super_chunks = n_super_chunks)

    v = np.random.rand(nrows)
    v_dref = w.memory.put(value = v.data.cast('B'), eager_alloc = 1)
    t.report('gen v')

    async def build_matrix(w, args):
        start_row, end_row = args
        A = make_test_matrix(end_row - start_row, 1)
        out = np.empty(end_row - start_row)

        matrix = (A.indptr, A.indices, A.data, out)
        matrix_dref = tsk.put(w, value = matrix)

        # v_dref_out = v_dref
        # vs = []
        # for vc in v_chunks:
        #     vs.append(np.frombuffer(await tsk.remote_get(w, vc), dtype = np.float64))
        # v = np.concatenate(vs)
        v_buf = await tsk.remote_get(w, v_dref)
        v = np.frombuffer(v_buf, dtype = np.float64).copy()
        v_dref_out = tsk.put(w, value = v)

        return (v_dref_out, matrix_dref)
    build_dref = put_fnc(w, build_matrix)

    matrix_chunks = await map(w, build_dref, nrows, n_super_chunks = n_super_chunks)
    t.report('distribute matrix')

    async def dot(w, args):
        t = tsk.Timer(output_fnc = lambda x: None)
        if w.addr == 10:
            t = tsk.Timer()
        istart, iend, st = args
        assert(iend == istart + 1)
        # st, i = args
        v_dref, matrix_dref = matrix_chunks[istart]
        # print(w.addr, 'took', time.time() - st, 'to launch')

        indptr, indices, data, out = w.memory.get_local(matrix_dref)
        v = w.memory.get_local(v_dref)
        # v_buf = await tsk.remote_get(w, v_dref)
        # v = np.frombuffer(v_buf, dtype = np.float64)

        inner_chunk_size = int(1e9)
        t.report(str(w.addr) + ' setup')

        for i in range(0, indptr.shape[0], inner_chunk_size):
            _sparse.csrmv(
                indptr[i:(i + inner_chunk_size + 1)],
                indices,
                data, v, out[i:(i+inner_chunk_size)],
                False
            )
            await asyncio.sleep(0)
        t.report(str(w.addr) + ' dot')
        # return out_dref

    dot_dref = put_fnc(w, dot)

    # await run_dot()
    async with tsk.Profiler(w, range(0)):
        t.report('put/startprof')
        for i in range(4):
            print('')
            print('')
            print('')
            await map(w, dot_dref, len(matrix_chunks), time.time(), n_super_chunks = n_super_chunks)
            t.report('dot')

# tsk.cluster(n_cores, submit)
tsk.cluster(n_cores, submit2)
