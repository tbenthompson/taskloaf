import numpy as np
import taskloaf as tsk


async def submit():
    gang = await tsk.ctx().wait_for_workers(2)
    n = int(4e8)
    ref = tsk.alloc(n * 8)
    A = np.frombuffer(await ref.get(), dtype=np.float64)
    A[:] = np.random.rand(n)
    rhs = np.sum(A)
    for i in range(2):
        async with tsk.Profiler(gang):
            # ref = tsk.put(w, A.data.cast('B'))
            async def remote():
                A = np.frombuffer(await ref.get(), dtype=np.float64)
                return np.sum(A)

            lhs = await tsk.task(remote, to=gang[1])
            assert lhs == rhs


def setup_worker(name):
    tsk.cfg.stdout_logging(name, "taskloaf.profile")


if __name__ == "__main__":
    cfg = tsk.Cfg(initializer=setup_worker)
    tsk.zmq_run(cfg=cfg, f=submit)
