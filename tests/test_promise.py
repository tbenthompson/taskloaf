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
        assert(await task(w, there, to = 1) == 13)

        thirteen = taskloaf.serialize.dumps(13)
        def there2(w):
            assert(w.addr == 1)
            return thirteen
        assert(await task(w, there2, to = 1) == thirteen)

        there_bytes = w.memory.put(serialized = taskloaf.serialize.dumps(there))
        assert(await task(w, there_bytes, to = 1) == 13)

        def f_with_args(w, a):
            return a
        f_bytes = w.memory.put(serialized = taskloaf.serialize.dumps(f_with_args))
        assert(await task(w, f_bytes, 14, to = 1) == 14)
        a_bytes = w.memory.put(serialized = taskloaf.serialize.dumps(15))
        assert(await task(w, f_bytes, a_bytes, to = 1) == 15)

    cluster(2, f)

import asyncio
@mpi_procs(2)
def test_then():
    async def f(w):
        y = await task(w, lambda w: 10, to = 1).then(lambda w, x: 2 * x, to = 0)
        assert(y == 20)
    cluster(2, f)

@mpi_procs(2)
def test_auto_unwrap():
    async def f(w):
        y = await task(w, lambda w: 10, to = 1).then(
            lambda w, x: task(w, lambda w: 2 * x),
            to = 0
        )
        assert(y == 20)
    cluster(2, f)

@mpi_procs(2)
def test_when_all():
    async def f(w):
        y = await when_all([
            task(w, lambda w: 10),
            task(w, lambda w: 5, to = 1)
        ]).then(lambda w, x: sum(x), to = 1)
        assert(y == 15)
    cluster(2, f)
