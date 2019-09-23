import taskloaf as tsk
import logging


async def submit():
    gang = await tsk.ctx().wait_for_workers(2)
    X = 3.1
    n = 10
    pr = tsk.task(lambda: tsk.task(lambda: X))
    for i in range(n):
        pr = pr.then(lambda x: x + 1)

    async def asum(x):
        raise Exception("E")
        return sum(x)

    pr = tsk.when_all([pr, tsk.task(lambda: X, to=gang[1])]).then(asum)
    print("answer is", await pr)


if __name__ == "__main__":
    cfg = tsk.Cfg(log_level=logging.WARN)
    tsk.zmq_run(cfg=cfg, f=submit)
