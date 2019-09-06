import taskloaf as tsk


async def submit():
    gang = list(tsk.ctx().messenger.endpts.keys())
    print(gang)

    X = 3.1
    n = 10
    pr = tsk.task(lambda: tsk.task(lambda: X))
    for i in range(n):
        pr = pr.then(lambda x: x + 1)
    pr = tsk.when_all([pr, tsk.task(lambda: X, to=gang[1])]).then(
        lambda x: sum(x)
    )
    print("answer is", await pr)


if __name__ == "__main__":
    tsk.run(submit)
