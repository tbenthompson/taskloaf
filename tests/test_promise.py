import pytest
import numpy as np
from taskloaf.promise import *
from taskloaf.test_decorators import mpi_procs
from taskloaf.run import null_comm_worker
from taskloaf.cluster import cluster

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
        assert(await task(w, there, to = 1) == 13)

        thirteen = taskloaf.serialize.dumps(w, 13)
        def there2(w):
            assert(w.addr == 1)
            return thirteen
        assert(await task(w, there2, to = 1) == thirteen)

        there_ref = put(w, there)
        assert(await task(w, there_ref, to = 1) == 13)

        def f_with_args(w, a):
            return a
        f_ref = put(w, f_with_args)
        assert(await task(w, f_ref, 14, to = 1) == 14)
        a_ref = put(w, 15)
        assert(await task(w, f_ref, a_ref, to = 1) == 15)

    cluster(2, f)

def test_task_await_elsewhere():
    async def f(w):
        def g(w):
            return 71
        pr = task(w, g, to = 1)
        async def h(w):
            a = await pr
            return a + 1
        pr2 = task(w, h, to = 2)
        assert(await pr2 == 72)
    cluster(3, f)

class Implosion(Exception):
    pass

def test_task_exception():
    async def f(w):
        def g(w):
            raise Implosion()

        with pytest.raises(Implosion):
            await task(w, g)

        with pytest.raises(Implosion):
            await task(w, g, to = 1)
    cluster(2, f)

def test_then():
    async def f(w):
        y = await task(w, lambda w: 10, to = 1).then(lambda w, x: 2 * x, to = 0)
        assert(y == 20)
    cluster(2, f)

def test_auto_unwrap():
    async def f(w):
        y = await task(w, lambda w: 10, to = 1).then(
            lambda w, x: task(w, lambda w: 2 * x),
            to = 0
        )
        assert(y == 20)
    cluster(2, f)

def test_when_all():
    async def f(w):
        y = await when_all([
            task(w, lambda w: 10),
            task(w, lambda w: 5, to = 1)
        ]).then(lambda w, x: sum(x), to = 1)
        assert(y == 15)
    cluster(2, f)

# When originally written, this test hung forever. So, even though there are no
# asserts, it is testing *something*. It checks that the exception behavior
# propagates from free tasks upward to the Worker properly.
# def test_cluster_broken_task():
#     async def f(w):
#
#         async def f(w):
#             while True:
#                 await asyncio.sleep(0)
#
#         async def broken_task(w):
#             print(x)
#
#         taskloaf.task(w, f)
#         await taskloaf.task(w, broken_task, to = 1)
#     cluster(2, f)

