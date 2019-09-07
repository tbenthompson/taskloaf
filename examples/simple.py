import asyncio
import taskloaf as tsk


async def f():
    await asyncio.sleep(1)
    print("HI")
    gang = list(tsk.ctx().messenger.endpts.keys())
    print(gang)

    print("LAUNCH")
    await tsk.task(lambda: print("hI"), to=gang[0])
    print("DONE")
    return 123


if __name__ == "__main__":
    r = tsk.run(f)
    print(r)
    assert r == 123
