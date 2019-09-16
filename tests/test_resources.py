import os
import taskloaf as tsk


async def f():
    print(f"f-worker name: {tsk.ctx().name}")
    gang = await tsk.ctx().wait_for_workers(2)
    return await tsk.task(lambda: 99, to=gang[0])


def setup_worker(name):
    tsk.cfg.stdout_logging(name, "taskloaf.shmem")


def test_delete_shm():
    initial_dev_shm = os.listdir("/dev/shm")

    cfg = tsk.Cfg(initializer=setup_worker)
    r = tsk.zmq_run(cfg=cfg, f=f)
    assert r == 99

    final_dev_shm = os.listdir("/dev/shm")
    new_files = set(final_dev_shm) - set(initial_dev_shm)
    assert len(new_files) == 0
