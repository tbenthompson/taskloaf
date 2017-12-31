import numpy as np
from taskloaf.promise import *
from taskloaf.test_decorators import mpi_procs
from taskloaf.run import null_comm_worker
from taskloaf.cluster import cluster

@mpi_procs(2)
def test_task():
    async def f(w):
        w.abc = 0
        def here(w):
            w.abc = 1
        await task(w, here)
        assert(w.abc == 1)

        def here_args(w, x):
            w.abc = x
        await task(w, here_args, 17)
        assert(w.abc == 17)

        def there(w):
            assert(w.addr == 1)
            return 13
        assert(await task(w, there, to = 1) == 1)
    cluster(2, f)


# test task, then, unwrap, when_all, delete_after_triggered

# def test_task_encode():
#     w = null_comm_worker()
#     coded = task_encoder(lambda x: 15, Promise(w, 0), True, 123)
#     f, pr, has_args, args = task_decoder(w, memoryview(coded)[8:])
#     assert(f(0) == 15)
#     assert(pr.dref.owner == 0)
#     assert(has_args)
#     assert(args == 123)
#
# def test_setresult_encode():
#     w = null_comm_worker()
#     coded = set_result_encoder(Promise(w, 0), np.array([0,1,2]))
#     pr, v = set_result_decoder(w, memoryview(coded)[8:])
#     np.testing.assert_almost_equal(v, [0,1,2])
#     assert(pr.dref.owner == 0)
#
# def test_await_encode():
#     w = null_comm_worker()
#     coded = await_encoder(Promise(w, 0), 199)
#     pr, req_addr = await_decoder(w, memoryview(coded)[8:])
#     assert(req_addr == 199)
#     assert(pr.dref.owner == 0)
