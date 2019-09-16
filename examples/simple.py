import taskloaf as tsk

N = 4


async def f():
    gang = await tsk.ctx().wait_for_workers(N)
    await tsk.task(lambda: print("hI"), to=gang[N - 2])
    return 123


if __name__ == "__main__":
    r = tsk.zmq_run(cfg=tsk.Cfg(n_workers=N), f=f)
    print(r)
    assert r == 123
