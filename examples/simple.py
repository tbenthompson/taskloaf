import asyncio
import taskloaf as tsk


async def f():
    while len(tsk.ctx().messenger.endpts.keys()) < 2:
        await asyncio.sleep(0)
    # print("HI")
    gang = list(tsk.ctx().messenger.endpts.keys())
    print(gang)
    print(gang)
    print(gang)
    print(gang)
    print(gang)
    print(gang)

    print("LAUNCH")
    await tsk.task(lambda: print("hI"), to=gang[0])
    print("DONE")
    return 123


if __name__ == "__main__":
    cfg = tsk.Cfg()
    cfg.n_workers = 3
    r = tsk.zmq_run(cfg=cfg, f=f)
    print(r)
    assert r == 123
